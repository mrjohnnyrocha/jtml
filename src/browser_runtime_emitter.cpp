#include "jtml/browser_runtime_emitter.h"

#include <sstream>

namespace jtml {

std::string emitBrowserRuntimeScript(int webSocketPort, bool browserLocalRuntime) {
    // The runtime script has three layers, in order of resilience:
    //
    //   1. `window.__jtml_bindings`  — optional JSON blob injected by the
    //      server before this script tag. Applied on DOMContentLoaded so the
    //      page renders with correct initial values even if no network is
    //      available.
    //   2. WebSocket — watch-mode reloads and future streaming updates.
    //   3. HTTP event dispatch — `fetch('/api/event', ...)`. This is the
    //      authoritative path for user events because it returns a complete
    //      bindings snapshot immediately.
    //
    // Events (`sendEvent`) use HTTP so local state, stores, component
    // instances, and conditionals are deterministic in every browser.
    std::ostringstream out;
    out << R"(
  <script>
    (function () {
      const wsPort = )" << webSocketPort << R"(;
      const browserLocalRuntime = )" << (browserLocalRuntime ? "true" : "false") << R"(;

      function reportStatus(state, message) {
        document.documentElement.dataset.jtmlStatus = state;
        if (message) document.documentElement.dataset.jtmlMessage = message;
        if (state === 'error') console.error('[jtml] ' + (message || 'runtime error'));
        try {
          if (window.parent && window.parent !== window) {
            window.parent.postMessage({
              type: 'jtml:runtime-status',
              state: state,
              message: message || ''
            }, '*');
          }
        } catch (_) {}
      }

      const clientState = {};
      const clientDerived = {};
      const clientActions = {};
      const __jtml_extern_fns = {};
      const __jtml_media_actions = {};
      let componentInstances = [];
      let componentDefinitions = [];
      const componentScopes = {};

      window.jtmlEventValue = function (event) {
        const target = event && event.target;
        if (!target) return '';
        const type = String(target.type || '').toLowerCase();
        if (type === 'checkbox') return !!target.checked;
        if (type === 'file') {
          const files = Array.prototype.slice.call(target.files || []).map(function (file) {
            const preview = (file && typeof URL !== 'undefined' && URL.createObjectURL)
              ? URL.createObjectURL(file)
              : '';
            return {
              name: file.name || '',
              type: file.type || '',
              size: file.size || 0,
              lastModified: file.lastModified || 0,
              preview: preview,
              url: preview
            };
          });
          if (target.multiple) return files;
          return files[0] || null;
        }
        return target.value;
      };

      function parseComponentMap(value) {
        const map = {};
        String(value || '').split(';').forEach(function (entry) {
          if (!entry) return;
          const pos = entry.indexOf('=');
          if (pos === -1) {
            map[entry] = '';
            return;
          }
          map[entry.slice(0, pos)] = entry.slice(pos + 1);
        });
        return map;
      }

      function escapeSelectorValue(value) {
        if (window.CSS && CSS.escape) return CSS.escape(value);
        return String(value || '').replace(/["\\]/g, '\\$&');
      }

      function componentElementFor(id) {
        if (!id) return null;
        return document.querySelector('[data-jtml-instance="' + escapeSelectorValue(id) + '"]');
      }

      function componentDefinitionElementFor(name) {
        if (!name) return null;
        return document.querySelector('[data-jtml-component-def="' + escapeSelectorValue(name) + '"]');
      }

      function splitComponentWords(source) {
        const words = [];
        let current = '';
        let quote = '';
        source = String(source || '');
        for (let i = 0; i < source.length; i += 1) {
          const ch = source[i];
          if (quote) {
            current += ch;
            if (ch === '\\' && i + 1 < source.length) current += source[++i];
            else if (ch === quote) quote = '';
            continue;
          }
          if (ch === '"' || ch === "'") {
            quote = ch;
            current += ch;
            continue;
          }
          if (/\s/.test(ch)) {
            if (current) {
              words.push(current);
              current = '';
            }
          } else {
            current += ch;
          }
        }
        if (current) words.push(current);
        return words;
      }

      function unquoteComponentText(value) {
        value = String(value || '').trim();
        if ((value[0] === '"' && value[value.length - 1] === '"') ||
            (value[0] === "'" && value[value.length - 1] === "'")) {
          return value.slice(1, -1);
        }
        return value;
      }

      function escapeHtml(value) {
        return String(value == null ? '' : value)
          .replace(/&/g, '&amp;')
          .replace(/</g, '&lt;')
          .replace(/>/g, '&gt;')
          .replace(/"/g, '&quot;');
      }

      function componentTagName(name) {
        const map = {
          page: 'main',
          box: 'div',
          stack: 'div',
          cluster: 'div',
          split: 'div',
          grid: 'div',
          card: 'section',
          panel: 'section',
          text: 'p',
          title: 'h1',
          subtitle: 'p'
        };
        return map[name] || name;
      }

      function componentScopeFor(instance, definition) {
        if (!instance || !instance.id) return {};
        if (componentScopes[instance.id]) return componentScopes[instance.id];
        const scope = {};
        Object.assign(scope, instance.params || {});
        (definition.bodyPlan || []).forEach(function (node) {
          if (!node || node.kind !== 'state' || !node.name || node.parentIndex !== -1) return;
          const result = evaluateClientBodyExpression(node.expression || '', scope);
          scope[node.name] = result.found ? result.value : unquoteComponentText(node.expression || '');
        });
        componentScopes[instance.id] = scope;
        return scope;
      }

      function applyComponentDerived(scope, definition) {
        (definition.bodyPlan || []).forEach(function (node) {
          if (!node || node.kind !== 'derived' || !node.name) return;
          const result = evaluateClientBodyExpression(node.expression || '', scope);
          if (result.found) scope[node.name] = result.value;
        });
      }

      function componentNodeChildren(definition, node) {
        return (node.childIndices || [])
          .map(function (index) { return (definition.bodyPlan || [])[index]; })
          .filter(Boolean);
      }

      function renderComponentExpression(expr, scope) {
        const literal = unquoteComponentText(expr);
        if (literal !== String(expr || '').trim()) return literal;
        const result = evaluateClientExpression(expr, scope);
        return result.found ? renderTemplateValue(result.value) : literal;
      }

      function renderComponentPlanNode(definition, instance, node, scope) {
        if (!node || node.kind !== 'template') return '';
        const words = splitComponentWords(node.text || '');
        const head = node.name || words[0] || 'box';
        const children = componentNodeChildren(definition, node)
          .map(function (child) { return renderComponentPlanNode(definition, instance, child, scope); })
          .join('');
        if (head === 'slot') return '';
        if (head === 'if') {
          const condition = evaluateClientExpression(node.expression || words.slice(1).join(' '), scope);
          return condition.found && condition.value ? children : '';
        }
        if (head === 'for') {
          const raw = node.expression || words.slice(1).join(' ');
          const match = String(raw || '').match(/^([A-Za-z_][A-Za-z0-9_]*)\s+in\s+(.+)$/);
          if (!match) return '';
          const result = evaluateClientExpression(match[2], scope);
          if (!result.found) return '';
          let values = result.value;
          if (values == null) values = [];
          if (!Array.isArray(values)) {
            if (typeof values === 'string') values = values.split('');
            else if (typeof values === 'object') values = Object.values(values);
            else values = [values];
          }
          return values.map(function (item) {
            const childScope = Object.assign({}, scope);
            childScope[match[1]] = item;
            return componentNodeChildren(definition, node)
              .map(function (child) {
                return renderComponentPlanNode(definition, instance, child, childScope);
              })
              .join('');
          }).join('');
        }
        if (head === 'show' || head === 'text') {
          const expr = head === 'show'
            ? (node.expression || words.slice(1).join(' '))
            : (node.expression || words.slice(1).join(' '));
          return '<p>' + escapeHtml(renderComponentExpression(expr, scope)) + '</p>' + children;
        }
        if (head === 'button') {
          const clickIndex = words.indexOf('click');
          const labelExpr = clickIndex === -1
            ? words.slice(1).join(' ')
            : words.slice(1, clickIndex).join(' ');
          const actionName = clickIndex === -1 ? '' : unquoteComponentText(words[clickIndex + 1] || '');
          const label = renderComponentExpression(labelExpr || '"Button"', scope);
          const attrs = actionName
            ? ' data-jtml-direct-component-id="' + escapeHtml(instance.id) + '"' +
              ' data-jtml-direct-component-action="' + escapeHtml(actionName) + '"'
            : '';
          return '<button type="button"' + attrs + '>' + escapeHtml(label) + '</button>' + children;
        }
        const tag = componentTagName(head);
        const contentExpr = (node.expression || '').trim();
        const inline = children ? '' : (contentExpr ? escapeHtml(renderComponentExpression(contentExpr, scope)) : '');
        return '<' + tag + ' data-jtml-direct-node="' + escapeHtml(head) + '">' +
          inline + children + '</' + tag + '>';
      }

      function runComponentPlanStatements(definition, scope, nodes) {
        (nodes || []).forEach(function (node) {
          if (!node) return;
          if (node.kind === 'assignment') {
            const result = evaluateClientBodyExpression(node.expression || '', scope);
            const next = result.found ? result.value : unquoteComponentText(node.expression || '');
            if (node.operator === '+=') scope[node.name] = Number(scope[node.name] || 0) + Number(next);
            else if (node.operator === '-=') scope[node.name] = Number(scope[node.name] || 0) - Number(next);
            else if (node.operator === '*=') scope[node.name] = Number(scope[node.name] || 0) * Number(next);
            else if (node.operator === '/=') scope[node.name] = Number(scope[node.name] || 0) / Number(next);
            else scope[node.name] = next;
          }
          runComponentPlanStatements(definition, scope, componentNodeChildren(definition, node));
        });
      }

      function renderDirectComponent(instance) {
        if (!instance || !instance.element) return false;
        const definition = componentDefinitions.find(function (item) {
          return item.name === instance.component;
        });
        if (!definition || !Array.isArray(definition.bodyPlan) || !definition.bodyPlan.length) {
          return false;
        }
        const roots = definition.bodyPlan.filter(function (node) {
          return node && node.renderRoot && node.kind === 'template';
        });
        if (!roots.length) return false;
        const scope = componentScopeFor(instance, definition);
        applyComponentDerived(scope, definition);
        instance.scope = scope;
        instance.element.innerHTML = roots.map(function (node) {
          return renderComponentPlanNode(definition, instance, node, scope);
        }).join('');
        instance.element.dataset.jtmlDirectRendered = 'true';
        return true;
      }

      function renderDirectComponents() {
        let rendered = 0;
        componentInstances.forEach(function (instance) {
          if (renderDirectComponent(instance)) rendered += 1;
        });
        window.jtml = Object.assign(window.jtml || {}, {
          directComponentExecution: true,
          directComponentRenderCount: rendered
        });
      }

      function runDirectComponentAction(componentId, actionName) {
        const instance = componentInstances.find(function (item) { return item.id === componentId; });
        if (!instance) return false;
        const definition = componentDefinitions.find(function (item) {
          return item.name === instance.component;
        });
        if (!definition || !Array.isArray(definition.bodyPlan)) return false;
        const actionNode = definition.bodyPlan.find(function (node) {
          return node && node.kind === 'action' && node.name === actionName;
        });
        if (!actionNode) return false;
        const scope = componentScopeFor(instance, definition);
        runComponentPlanStatements(definition, scope, componentNodeChildren(definition, actionNode));
        renderDirectComponent(instance);
        return true;
      }

      function startDirectComponentBindings() {
        if (document.documentElement.dataset.jtmlDirectComponentBindings === 'true') return;
        document.documentElement.dataset.jtmlDirectComponentBindings = 'true';
        document.addEventListener('click', function (event) {
          const button = event.target && event.target.closest &&
            event.target.closest('[data-jtml-direct-component-action]');
          if (!button) return;
          const componentId = button.getAttribute('data-jtml-direct-component-id') || '';
          const actionName = button.getAttribute('data-jtml-direct-component-action') || '';
          if (runDirectComponentAction(componentId, actionName)) {
            event.preventDefault();
            event.stopPropagation();
          }
        });
      }

      function scanComponentInstances() {
        const manifestInstances = (window.jtml && Array.isArray(window.jtml.componentInstanceManifest))
          ? window.jtml.componentInstanceManifest
          : [];
        const manifestDefinitions = (window.jtml && Array.isArray(window.jtml.componentDefinitionManifest))
          ? window.jtml.componentDefinitionManifest
          : [];
        if (manifestInstances.length) {
          componentInstances = manifestInstances.map(function (record) {
            return {
              id: record.id || '',
              component: record.component || '',
              instanceId: Number(record.instanceId || 0),
              role: record.role || 'component',
              params: record.params || {},
              locals: record.locals || {},
              sourceLine: Number(record.sourceLine || 0),
              element: componentElementFor(record.id || '')
            };
          });
        } else {
          componentInstances = Array.prototype.slice.call(
            document.querySelectorAll('[data-jtml-instance]')
          ).map(function (el) {
            return {
              id: el.getAttribute('data-jtml-instance') || '',
              component: el.getAttribute('data-jtml-component') || '',
              instanceId: Number(el.getAttribute('data-jtml-instance-id') || 0),
              role: el.getAttribute('data-jtml-component-role') || 'component',
              params: parseComponentMap(el.getAttribute('data-jtml-component-params') || ''),
              locals: parseComponentMap(el.getAttribute('data-jtml-component-locals') || ''),
              sourceLine: Number(el.getAttribute('data-jtml-source-line') || 0),
              element: el
            };
          });
        }
        if (manifestDefinitions.length) {
          componentDefinitions = manifestDefinitions.map(function (record) {
            return {
              name: record.name || '',
              params: Array.isArray(record.params) ? record.params.slice() : [],
              localState: Array.isArray(record.localState) ? record.localState.slice() : [],
              localDerived: Array.isArray(record.localDerived) ? record.localDerived.slice() : [],
              localActions: Array.isArray(record.localActions) ? record.localActions.slice() : [],
              localEffects: Array.isArray(record.localEffects) ? record.localEffects.slice() : [],
              eventBindings: Array.isArray(record.eventBindings) ? record.eventBindings.slice() : [],
              hasSlot: !!record.hasSlot,
              bodyNodeCount: Number(record.bodyNodeCount || 0),
              rootTemplateNodeCount: Number(record.rootTemplateNodeCount || 0),
              slotCount: Number(record.slotCount || 0),
              bodyPlan: Array.isArray(record.bodyPlan) ? record.bodyPlan.slice() : [],
              runtimePlan: {
                mode: 'semantic-instance',
                ownsEnvironment: true,
                bodyNodeCount: Number(record.bodyNodeCount || 0),
                rootTemplateNodeCount: Number(record.rootTemplateNodeCount || 0),
                slotCount: Number(record.slotCount || 0),
                bodyPlan: Array.isArray(record.bodyPlan) ? record.bodyPlan.slice() : [],
                actions: Array.isArray(record.localActions) ? record.localActions.slice() : [],
                state: Array.isArray(record.localState) ? record.localState.slice() : [],
                derived: Array.isArray(record.localDerived) ? record.localDerived.slice() : [],
                effects: Array.isArray(record.localEffects) ? record.localEffects.slice() : []
              },
              sourceLine: Number(record.sourceLine || 0),
              body: decodeHex(record.bodyHex || ''),
              element: componentDefinitionElementFor(record.name || '')
            };
          });
        } else {
          componentDefinitions = Array.prototype.slice.call(
            document.querySelectorAll('[data-jtml-component-def]')
          ).map(function (el) {
            return {
              name: el.getAttribute('data-jtml-component-def') || '',
              params: String(el.getAttribute('data-jtml-component-def-params') || '').split(';').filter(Boolean),
              sourceLine: Number(el.getAttribute('data-jtml-source-line') || 0),
              body: decodeHex(el.getAttribute('data-jtml-component-body-hex') || ''),
              runtimePlan: {
                mode: 'expanded-compatibility',
                ownsEnvironment: true,
                bodyNodeCount: 0,
                rootTemplateNodeCount: 0,
                slotCount: 0,
                actions: [],
                state: [],
                derived: [],
                effects: [],
                bodyPlan: []
              },
              bodyPlan: [],
              element: el
            };
          });
        }
        componentInstances.forEach(function (instance) {
          const definition = componentDefinitions.find(function (item) {
            return item.name === instance.component;
          }) || {};
          instance.runtime = {
            mode: 'semantic-instance',
            ownsEnvironment: true,
            ready: !!instance.id,
            environmentId: instance.instanceId,
            definition: instance.component,
            actions: Array.isArray(definition.localActions) ? definition.localActions.slice() : [],
            state: Array.isArray(definition.localState) ? definition.localState.slice() : [],
            derived: Array.isArray(definition.localDerived) ? definition.localDerived.slice() : [],
            effects: Array.isArray(definition.localEffects) ? definition.localEffects.slice() : [],
            hasSlot: !!definition.hasSlot,
            bodyNodeCount: Number(definition.bodyNodeCount || 0),
            rootTemplateNodeCount: Number(definition.rootTemplateNodeCount || 0)
          };
        });
        window.__jtml_components = componentInstances;
        window.__jtml_component_definitions = componentDefinitions;
        window.jtml = Object.assign(window.jtml || {}, {
          components: componentInstances,
          componentDefinitions: componentDefinitions,
          getComponentInstances: function () { return componentInstances.slice(); },
          getComponentDefinitions: function () { return componentDefinitions.slice(); },
          findComponentInstance: function (id) {
            return componentInstances.find(function (item) { return item.id === id; }) || null;
          },
          findComponentDefinition: function (name) {
            return componentDefinitions.find(function (item) { return item.name === name; }) || null;
          }
        });
        document.dispatchEvent(new CustomEvent('jtml:components-ready', {
          detail: { components: componentInstances, componentDefinitions: componentDefinitions }
        }));
      }

      function decodeHex(hex) {
        hex = String(hex || '');
        if (hex.length % 2 !== 0) return '';
        let out = '';
        for (let i = 0; i < hex.length; i += 2) {
          const code = parseInt(hex.slice(i, i + 2), 16);
          if (!Number.isFinite(code)) return '';
          out += String.fromCharCode(code);
        }
        return out;
      }

      function normalizeClientExpr(expr) {
        expr = String(expr || '').trim();
        const unwrapPaths = function (value) {
          let previous = '';
          while (previous !== value) {
            previous = value;
            value = value.replace(/\(([A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*)\)/g, '$1');
          }
          return value;
        };
        const hasWrappingParens = function (value) {
          if (value.length < 3 || value[0] !== '(' || value[value.length - 1] !== ')') return false;
          let depth = 0;
          for (let i = 0; i < value.length; i += 1) {
            const ch = value[i];
            if (ch === '(') depth += 1;
            else if (ch === ')') depth -= 1;
            if (depth === 0 && i < value.length - 1) return false;
          }
          return depth === 0 && value.slice(1, -1).trim().length > 0;
        };
        expr = unwrapPaths(expr);
        while (hasWrappingParens(expr)) {
          expr = expr.slice(1, -1).trim();
          expr = unwrapPaths(expr);
        }
        return expr;
      }

      function deepGet(scope, path) {
        const parts = normalizeClientExpr(path).split('.').filter(Boolean);
        if (!parts.length || !Object.prototype.hasOwnProperty.call(scope, parts[0])) {
          return { found: false, value: undefined };
        }
        let value = scope[parts[0]];
        for (let i = 1; i < parts.length; i += 1) {
          if (value == null || !Object.prototype.hasOwnProperty.call(Object(value), parts[i])) {
            return { found: false, value: undefined };
          }
          value = value[parts[i]];
        }
        return { found: true, value: value };
      }

      function splitTopLevelToken(source, token) {
        const parts = [];
        let current = '';
        let quote = '';
        let depth = 0;
        for (let i = 0; i < source.length; i += 1) {
          const ch = source[i];
          if (quote) {
            current += ch;
            if (ch === '\\' && i + 1 < source.length) current += source[++i];
            else if (ch === quote) quote = '';
            continue;
          }
          if (ch === '"' || ch === "'") {
            quote = ch;
            current += ch;
            continue;
          }
          if (ch === '{' || ch === '[' || ch === '(') depth += 1;
          if (ch === '}' || ch === ']' || ch === ')') depth -= 1;
          if (depth === 0 && source.slice(i, i + token.length) === token) {
            parts.push(current.trim());
            current = '';
            i += token.length - 1;
          } else {
            current += ch;
          }
        }
        if (parts.length) parts.push(current.trim());
        return parts.filter(function (part) { return part.length > 0; });
      }

      function evaluateClientExpression(expr, scope) {
        expr = normalizeClientExpr(expr);
        if (!expr) return { found: false, value: undefined };
        const conditionalParts = splitTopLevelToken(expr, '?');
        if (conditionalParts.length === 2) {
          const falseParts = splitTopLevelToken(conditionalParts[1], ':');
          if (falseParts.length === 2) {
            const condition = evaluateClientExpression(conditionalParts[0], scope);
            return evaluateClientExpression(condition.found && condition.value ? falseParts[0] : falseParts[1], scope);
          }
        }
        const orParts = splitTopLevelToken(expr, '||');
        if (orParts.length > 1) {
          for (let i = 0; i < orParts.length; i += 1) {
            const part = evaluateClientExpression(orParts[i], scope);
            if (!part.found) return { found: false, value: undefined };
            if (part.value) return { found: true, value: true };
          }
          return { found: true, value: false };
        }
        const andParts = splitTopLevelToken(expr, '&&');
        if (andParts.length > 1) {
          for (let i = 0; i < andParts.length; i += 1) {
            const part = evaluateClientExpression(andParts[i], scope);
            if (!part.found) return { found: false, value: undefined };
            if (!part.value) return { found: true, value: false };
          }
          return { found: true, value: true };
        }
        if (expr[0] === '!' && expr.slice(0, 2) !== '!=') {
          const part = evaluateClientExpression(expr.slice(1), scope);
          return part.found ? { found: true, value: !part.value } : part;
        }
        const comparisons = ['==', '!=', '>=', '<=', '>', '<'];
        for (let c = 0; c < comparisons.length; c += 1) {
          const op = comparisons[c];
          const parts = splitTopLevelToken(expr, op);
          if (parts.length === 2) {
            const left = evaluateClientExpression(parts[0], scope);
            const right = evaluateClientExpression(parts[1], scope);
            if (!left.found || !right.found) return { found: false, value: undefined };
            if (op === '==') return { found: true, value: left.value == right.value };
            if (op === '!=') return { found: true, value: left.value != right.value };
            if (op === '>=') return { found: true, value: left.value >= right.value };
            if (op === '<=') return { found: true, value: left.value <= right.value };
            if (op === '>') return { found: true, value: left.value > right.value };
            if (op === '<') return { found: true, value: left.value < right.value };
          }
        }
        const plusParts = splitTopLevelOperator(expr, '+');
        if (plusParts.length > 1) {
          let out = '';
          for (let i = 0; i < plusParts.length; i += 1) {
            const part = evaluateClientExpression(plusParts[i], scope);
            if (!part.found) {
              return { found: false, value: undefined };
            }
            out += renderTemplateValue(part.value);
          }
          return { found: true, value: out };
        }
        const minusParts = splitTopLevelToken(expr, '-');
        if (minusParts.length === 2) {
          const left = evaluateClientExpression(minusParts[0], scope);
          const right = evaluateClientExpression(minusParts[1], scope);
          if (left.found && right.found) return { found: true, value: Number(left.value) - Number(right.value) };
        }
        const multiplyParts = splitTopLevelToken(expr, '*');
        if (multiplyParts.length === 2) {
          const left = evaluateClientExpression(multiplyParts[0], scope);
          const right = evaluateClientExpression(multiplyParts[1], scope);
          if (left.found && right.found) return { found: true, value: Number(left.value) * Number(right.value) };
        }
        const divideParts = splitTopLevelToken(expr, '/');
        if (divideParts.length === 2) {
          const left = evaluateClientExpression(divideParts[0], scope);
          const right = evaluateClientExpression(divideParts[1], scope);
          if (left.found && right.found) return { found: true, value: Number(left.value) / Number(right.value) };
        }
        if (expr === 'true') return { found: true, value: true };
        if (expr === 'false') return { found: true, value: false };
        if ((expr[0] === '"' && expr[expr.length - 1] === '"') ||
            (expr[0] === "'" && expr[expr.length - 1] === "'")) {
          return { found: true, value: expr.slice(1, -1) };
        }
        if (/^-?\d+(?:\.\d+)?$/.test(expr)) {
          return { found: true, value: Number(expr) };
        }
        if (/^[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*$/.test(expr)) {
          if (scope) {
            const scoped = deepGet(scope, expr);
            if (scoped.found) return scoped;
          }
          return deepGet(clientState, expr);
        }
        return { found: true, value: expr };
      }

      function splitTopLevelOperator(source, op) {
        const parts = [];
        let current = '';
        let quote = '';
        let depth = 0;
        for (let i = 0; i < source.length; i += 1) {
          const ch = source[i];
          if (quote) {
            current += ch;
            if (ch === '\\' && i + 1 < source.length) current += source[++i];
            else if (ch === quote) quote = '';
            continue;
          }
          if (ch === '"' || ch === "'") {
            quote = ch;
            current += ch;
            continue;
          }
          if (ch === '{' || ch === '[' || ch === '(') depth += 1;
          if (ch === '}' || ch === ']' || ch === ')') depth -= 1;
          if (ch === op && depth === 0) {
            parts.push(current.trim());
            current = '';
          } else {
            current += ch;
          }
        }
        if (parts.length) parts.push(current.trim());
        return parts.filter(function (part) { return part.length > 0; });
      }

      function splitTopLevelList(source) {
        const parts = [];
        let current = '';
        let quote = '';
        let depth = 0;
        for (let i = 0; i < source.length; i += 1) {
          const ch = source[i];
          if (quote) {
            current += ch;
            if (ch === '\\' && i + 1 < source.length) current += source[++i];
            else if (ch === quote) quote = '';
            continue;
          }
          if (ch === '"' || ch === "'") {
            quote = ch;
            current += ch;
            continue;
          }
          if (ch === '{' || ch === '[' || ch === '(') depth += 1;
          if (ch === '}' || ch === ']' || ch === ')') depth -= 1;
          if (ch === ',' && depth === 0) {
            if (current.trim()) parts.push(current.trim());
            current = '';
          } else {
            current += ch;
          }
        }
        if (current.trim()) parts.push(current.trim());
        return parts;
      }

      function evaluateClientBodyExpression(expr, scope) {
        expr = normalizeClientExpr(expr);
        if (!expr) return { found: false, value: undefined };
        if (expr[0] === '{' && expr[expr.length - 1] === '}') {
          const body = {};
          const inner = expr.slice(1, -1).trim();
          if (!inner) return { found: true, value: body };
          splitTopLevelList(inner).forEach(function (entry) {
            const colon = entry.indexOf(':');
            const rawKey = colon === -1 ? entry : entry.slice(0, colon).trim();
            const key = rawKey.replace(/^['"]|['"]$/g, '');
            const valueExpr = colon === -1 ? rawKey : entry.slice(colon + 1).trim();
            const value = evaluateClientExpression(valueExpr, scope);
            body[key] = value.found ? value.value : valueExpr;
          });
          return { found: true, value: body };
        }
        if (expr[0] === '[' && expr[expr.length - 1] === ']') {
          const inner = expr.slice(1, -1).trim();
          if (!inner) return { found: true, value: [] };
          return {
            found: true,
            value: splitTopLevelList(inner).map(function (part) {
              const value = evaluateClientBodyExpression(part, scope);
              return value.found ? value.value : part;
            })
          };
        }
        return evaluateClientExpression(expr, scope);
      }

      function applyClientDerived() {
        for (const name in clientDerived) {
          const result = evaluateClientBodyExpression(clientDerived[name]);
          if (result.found) clientState[name] = result.value;
        }
      }

      function applyClientState() {
        applyClientDerived();
        applyClientExpressions();
        applyClientAttributes();

        for (let pass = 0; pass < 5; pass += 1) {
          let changed = false;

          document.querySelectorAll('[data-jtml-cond-expr]').forEach(function (el) {
            const result = evaluateClientExpression(el.getAttribute('data-jtml-cond-expr'));
            if (!result.found) return;
            const source = result.value ? el.getAttribute('data-then') || el.getAttribute('data-body') || '' : el.getAttribute('data-else') || '';
            if (el.dataset.jtmlRendered !== source) {
              el.innerHTML = source;
              el.dataset.jtmlRendered = source;
              changed = true;
            }
          });

          document.querySelectorAll('[data-jtml-for-expr]').forEach(function (el) {
            const result = evaluateClientExpression(el.getAttribute('data-jtml-for-expr'));
            if (!result.found) return;
            const iterator = el.getAttribute('data-jtml-iterator') || 'item';
            const body = el.getAttribute('data-body') || '';
            let values = result.value;
            if (values == null) values = [];
            if (!Array.isArray(values)) {
              if (typeof values === 'string') values = values.split('');
              else if (typeof values === 'object') values = Object.values(values);
              else values = [values];
            }
            const html = values.map(function (item) { return renderLoopBody(body, iterator, item); }).join('');
            if (el.dataset.jtmlRendered !== html) {
              el.innerHTML = html;
              el.dataset.jtmlRendered = html;
              changed = true;
            }
          });

          applyClientExpressions();
          applyClientAttributes();
          if (!changed) break;
        }
        renderCharts();
        processImageBindings();
      }

      function applyClientExpressions() {
        document.querySelectorAll('[data-jtml-expr]').forEach(function (el) {
          const result = evaluateClientExpression(el.getAttribute('data-jtml-expr'));
          if (result.found) el.textContent = renderTemplateValue(result.value);
        });
      }

      function applyClientAttributes() {
        document.querySelectorAll('*').forEach(function (el) {
          Array.prototype.slice.call(el.attributes || []).forEach(function (attr) {
            const match = attr.name.match(/^data-jtml-attr-(.+)-expr$/);
            if (!match) return;
            const htmlAttr = match[1];
            const result = evaluateClientExpression(attr.value);
            if (!result.found) return;
            applyAttribute(el, htmlAttr, result.value);
          });
        });
      }

      function assignClientPath(path, value) {
        path = normalizeClientExpr(path);
        const parts = path.split('.').filter(Boolean);
        if (!parts.length) return;
        if (parts.length === 1) {
          clientState[parts[0]] = value;
          return;
        }
        let target = clientState[parts[0]];
        if (target == null || typeof target !== 'object') target = clientState[parts[0]] = {};
        for (let i = 1; i < parts.length - 1; i += 1) {
          if (target[parts[i]] == null || typeof target[parts[i]] !== 'object') target[parts[i]] = {};
          target = target[parts[i]];
        }
        target[parts[parts.length - 1]] = value;
      }

      function runClientStatements(statements) {
        statements = statements || [];
        for (let i = 0; i < statements.length; i += 1) {
          const stmt = statements[i];
          if (!stmt || !stmt.kind) continue;
          if (stmt.kind === 'assign') {
            const result = evaluateClientBodyExpression(stmt.expr || '');
            if (result.found) assignClientPath(stmt.lhs || '', result.value);
          } else if (stmt.kind === 'if') {
            const condition = evaluateClientExpression(stmt.condition || '');
            runClientStatements(condition.found && condition.value ? stmt.then : stmt.else);
          }
        }
      }

      function executeClientAction(name, args) {
        const action = clientActions[name];
        if (!action) return false;
        args = args || [];
        const restore = {};
        const missing = {};
        (action.params || []).forEach(function (param, index) {
          if (Object.prototype.hasOwnProperty.call(clientState, param)) restore[param] = clientState[param];
          else missing[param] = true;
          clientState[param] = args[index];
        });
        try {
          runClientStatements(action.body || []);
          applyClientState();
        } finally {
          (action.params || []).forEach(function (param) {
            if (Object.prototype.hasOwnProperty.call(restore, param)) clientState[param] = restore[param];
            else if (missing[param]) delete clientState[param];
          });
        }
        return true;
      }

      function ownProperty(object, key) {
        return Object.prototype.hasOwnProperty.call(Object(object || {}), key);
      }

      function mergeRuntimeBindings(target, bindings, overwrite) {
        if (!Array.isArray(bindings)) return target;
        bindings.forEach(function (binding) {
          if (!binding || !binding.name) return;
          if (overwrite || !ownProperty(target, binding.name)) {
            target[binding.name] = binding.expr || '';
          }
        });
        return target;
      }

      function mergeRuntimeActions(target, actions, overwrite) {
        if (!Array.isArray(actions)) return target;
        actions.forEach(function (action) {
          if (!action || !action.name) return;
          if (overwrite || !ownProperty(target, action.name)) {
            target[action.name] = {
              params: Array.isArray(action.params) ? action.params.slice() : [],
              body: Array.isArray(action.body) ? action.body.slice() : []
            };
          }
        });
        return target;
      }

      function mergeRuntimeArray(target, additions, keyOf, overwrite) {
        if (!Array.isArray(additions)) return target;
        additions.forEach(function (item) {
          if (!item) return;
          const key = keyOf(item);
          const index = target.findIndex(function (existing) {
            return keyOf(existing) === key;
          });
          if (index === -1) {
            target.push(item);
          } else if (overwrite) {
            target[index] = item;
          }
        });
        return target;
      }

      function mergeRuntimePlanIntoManifest(merged, plan, options) {
        if (!plan || typeof plan !== 'object') return;
        options = options || {};
        const preferDefinitions = !!options.preferDefinitions;
        mergeRuntimeBindings(merged.state, plan.state, false);
        mergeRuntimeBindings(merged.derived, plan.derived, false);
        mergeRuntimeActions(merged.actions, plan.actions, false);
        mergeRuntimeArray(merged.routes, plan.routes, function (route) {
          return String(route.path || '') + '|' + String(route.component || '');
        }, false);
        mergeRuntimeArray(merged.fetches, plan.fetches, function (fetchNode) {
          return String(fetchNode.name || '') + '|' + String(fetchNode.url || '');
        }, false);
        mergeRuntimeArray(merged.componentDefinitions, plan.componentDefinitions, function (definition) {
          return String(definition.name || '');
        }, preferDefinitions);
        mergeRuntimeArray(merged.componentInstances, plan.componentInstances, function (instance) {
          return String(instance.id || '') + '|' +
                 String(instance.component || '') + '|' +
                 String(instance.sourceLine || '');
        }, false);
      }

      function mergeProjectManifest(manifest) {
        manifest = manifest || {};
        const merged = Object.assign({}, manifest, {
          state: Object.assign({}, manifest.state || {}),
          derived: Object.assign({}, manifest.derived || {}),
          actions: Object.assign({}, manifest.actions || {}),
          routes: Array.isArray(manifest.routes) ? manifest.routes.slice() : [],
          fetches: Array.isArray(manifest.fetches) ? manifest.fetches.slice() : [],
          componentDefinitions: Array.isArray(manifest.componentDefinitions)
            ? manifest.componentDefinitions.slice()
            : [],
          componentInstances: Array.isArray(manifest.componentInstances)
            ? manifest.componentInstances.slice()
            : []
        });

        const project = manifest.project || {};
        mergeRuntimePlanIntoManifest(merged, project.linkedPlan, { preferDefinitions: false });
        if (Array.isArray(project.modules)) {
          project.modules.forEach(function (modulePlan) {
            mergeRuntimePlanIntoManifest(
              merged,
              modulePlan && modulePlan.plan,
              { preferDefinitions: true }
            );
          });
        }
        return merged;
      }

      function bootstrapClientManifest() {
        const el = document.getElementById('__jtml_client_manifest');
        if (!el) return;
        let manifest = null;
        try {
          manifest = JSON.parse(el.textContent || '{}');
        } catch (err) {
          reportStatus('error', 'Invalid browser runtime manifest');
          return;
        }
        manifest = mergeProjectManifest(manifest);
        const state = manifest.state || {};
        for (const name in state) {
          const result = evaluateClientBodyExpression(state[name]);
          if (result.found) clientState[name] = result.value;
        }
        const routeManifest = Array.isArray(manifest.routes) ? manifest.routes : [];
        const fetchManifest = Array.isArray(manifest.fetches) ? manifest.fetches : [];
        const componentDefinitionManifest = Array.isArray(manifest.componentDefinitions) ? manifest.componentDefinitions : [];
        const componentInstanceManifest = Array.isArray(manifest.componentInstances) ? manifest.componentInstances : [];
        Object.assign(clientDerived, manifest.derived || {});
        Object.assign(clientActions, manifest.actions || {});
        window.jtml = Object.assign(window.jtml || {}, {
          state: clientState,
          actions: clientActions,
          derived: clientDerived,
          routeManifest: routeManifest,
          fetchManifest: fetchManifest,
          componentDefinitionManifest: componentDefinitionManifest,
          componentInstanceManifest: componentInstanceManifest,
          projectManifest: manifest.project || null,
          runtimeManifestSource: manifest.project ? 'semantic-project' : 'linked-compatibility',
          runAction: function (name) {
            return executeClientAction(name, Array.prototype.slice.call(arguments, 1));
          }
        });
      }

      const __jtml_refresh_fns = {};
      const __jtml_fetch_fns = {};
      const __jtml_invalidate_fns = {};
      const __jtml_fetch_groups = {};
      const __jtml_fetch_timers = {};

      function createFetchState(previous, patch) {
        previous = previous || {};
        return Object.assign({
          loading: false,
          data: [],
          error: '',
          stale: false,
          attempts: 0,
          hasData: false,
          status: 0,
          ok: false,
          url: '',
          key: '',
          method: 'GET',
          updatedAt: 0
        }, previous, patch || {});
      }

      function resolveFetchUrl(url) {
        return String(url || '').replace(/\{([^{}]+)\}/g, function (match, expr) {
          const result = evaluateClientExpression(expr);
          if (!result.found || result.value == null) return '';
          return encodeURIComponent(renderTemplateValue(result.value));
        });
      }

      function resolveFetchKey(name, resolvedUrl, method, bodyExpr, cacheKeyExpr) {
        if (cacheKeyExpr) {
          const result = evaluateClientExpression(cacheKeyExpr);
          if (result.found) return String(renderTemplateValue(result.value));
          return String(cacheKeyExpr);
        }
        const body = bodyExpr ? evaluateClientBodyExpression(bodyExpr) : { found: false, value: '' };
        return JSON.stringify({
          name: name,
          url: resolvedUrl,
          method: method,
          body: body.found ? body.value : bodyExpr
        });
      }

      async function executeFetch(name, url, method, bodyExpr, cachePolicy, credentialsPolicy, timeoutMs, retryCount, stalePolicy, cacheKeyExpr, dedupe, force) {
        const resolvedUrl = resolveFetchUrl(url);
        const previous = createFetchState(clientState[name], {});
        const resolvedKey = resolveFetchKey(name, resolvedUrl, method, bodyExpr, cacheKeyExpr);
        if (dedupe && !force && previous.hasData && previous.key === resolvedKey && previous.url === resolvedUrl && previous.method === method) {
          return previous;
        }
        const keepStale = stalePolicy === 'keep';
        const hasPreviousData = !!previous.hasData;
        clientState[name] = createFetchState(previous, {
          loading: true,
          data: keepStale ? previous.data : [],
          error: '',
          stale: keepStale && hasPreviousData,
          hasData: keepStale && hasPreviousData,
          ok: false,
          url: resolvedUrl,
          key: resolvedKey,
          method: method
        });
        applyClientState();
        let lastError = null;
        let lastStatus = 0;
        const maxRetries = Math.max(0, Number(retryCount || 0) || 0);
        for (let attempt = 0; attempt <= maxRetries; attempt += 1) {
          const options = { method: method };
          let controller = null;
          let timeoutId = null;
          try {
            if (cachePolicy) options.cache = cachePolicy;
            if (credentialsPolicy) options.credentials = credentialsPolicy;
            if (timeoutMs) {
              controller = new AbortController();
              options.signal = controller.signal;
              timeoutId = setTimeout(function () { controller.abort(); }, Math.max(1, Number(timeoutMs) || 1));
            }
            if (bodyExpr) {
              const body = evaluateClientBodyExpression(bodyExpr);
              options.headers = Object.assign({ 'content-type': 'application/json' }, options.headers || {});
              options.body = JSON.stringify(body.found ? body.value : bodyExpr);
            }
            const response = await fetch(resolvedUrl, options);
            lastStatus = response.status;
            if (timeoutId) clearTimeout(timeoutId);
            const type = response.headers.get('content-type') || '';
            const payload = type.indexOf('application/json') !== -1 ? await response.json() : await response.text();
            if (!response.ok) throw new Error(response.status + ' ' + response.statusText);
            clientState[name] = createFetchState(previous, {
              loading: false,
              data: payload,
              error: '',
              stale: false,
              attempts: attempt + 1,
              hasData: true,
              status: response.status,
              ok: true,
              url: resolvedUrl,
              key: resolvedKey,
              method: method,
              updatedAt: Date.now()
            });
            lastError = null;
            break;
          } catch (err) {
            if (timeoutId) clearTimeout(timeoutId);
            lastError = err;
            if (attempt < maxRetries) continue;
          }
        }
        if (lastError) {
          clientState[name] = createFetchState(previous, {
            loading: false,
            data: keepStale ? previous.data : [],
            error: lastError && lastError.name === 'AbortError'
              ? 'Fetch timed out'
              : (lastError && lastError.message ? lastError.message : String(lastError)),
            stale: keepStale && hasPreviousData,
            attempts: maxRetries + 1,
            hasData: keepStale && hasPreviousData,
            status: lastStatus,
            ok: false,
            url: resolvedUrl,
            key: resolvedKey,
            method: method,
            updatedAt: Date.now()
          });
        }
        applyClientState();
      }

      function registerFetchBinding(record, lazy) {
        const name = record && record.name ? record.name : '';
        const url = record && record.url ? record.url : '';
        if (!name || !url || __jtml_fetch_fns[name]) return;
        const method = record.method || 'GET';
        const bodyExpr = record.bodyExpr || '';
        const refreshAction = record.refreshAction || '';
        const cachePolicy = record.cache || '';
        const credentialsPolicy = record.credentials || '';
        const timeoutMs = record.timeoutMs || '';
        const retryCount = record.retryCount || '';
        const stalePolicy = record.stalePolicy || 'clear';
        const group = record.group || '';
        const cacheKeyExpr = record.cacheKeyExpr || '';
        const revalidateMs = Math.max(0, Number(record.revalidateMs || 0) || 0);
        const dedupe = !!record.dedupe;
        const background = !!record.background;
        __jtml_fetch_fns[name] = function (force) {
          return executeFetch(name, url, method, bodyExpr, cachePolicy, credentialsPolicy, timeoutMs, retryCount, stalePolicy, cacheKeyExpr, dedupe, !!force);
        };
        if (group) {
          if (!__jtml_fetch_groups[group]) __jtml_fetch_groups[group] = [];
          if (__jtml_fetch_groups[group].indexOf(name) === -1) __jtml_fetch_groups[group].push(name);
        }
        if (refreshAction) {
          __jtml_refresh_fns[refreshAction] = function () {
            return __jtml_fetch_fns[name](true);
          };
        }
        if (revalidateMs > 0 && !__jtml_fetch_timers[name]) {
          __jtml_fetch_timers[name] = setInterval(function () {
            if (!background && typeof document !== 'undefined' && document.hidden) return;
            if (__jtml_fetch_fns[name]) __jtml_fetch_fns[name](true);
          }, revalidateMs);
        }
        if (!lazy) __jtml_fetch_fns[name](false);
      }

      function startFetchBindings() {
        const manifestFetches = (window.jtml && Array.isArray(window.jtml.fetchManifest))
          ? window.jtml.fetchManifest
          : [];
        manifestFetches.forEach(function (fetch) {
          registerFetchBinding(fetch, !!fetch.lazy);
        });
        document.querySelectorAll('[data-jtml-fetch]').forEach(function (marker) {
          registerFetchBinding({
            name: marker.getAttribute('data-jtml-fetch') || '',
            url: marker.getAttribute('data-url') || '',
            method: marker.getAttribute('data-method') || 'GET',
            bodyExpr: marker.getAttribute('data-body-expr') || '',
            refreshAction: marker.getAttribute('data-refresh-action') || '',
            cache: marker.getAttribute('data-cache') || '',
            credentials: marker.getAttribute('data-credentials') || '',
            timeoutMs: marker.getAttribute('data-timeout-ms') || '',
            retryCount: marker.getAttribute('data-retry') || '',
            stalePolicy: marker.getAttribute('data-stale') || 'clear',
            group: marker.getAttribute('data-group') || '',
            cacheKeyExpr: marker.getAttribute('data-cache-key-expr') || '',
            revalidateMs: marker.getAttribute('data-revalidate-ms') || '',
            dedupe: marker.getAttribute('data-dedupe') === 'true',
            background: marker.getAttribute('data-background') === 'true'
          }, marker.getAttribute('data-lazy') === 'true');
        });
        window.jtml = Object.assign(window.jtml || {}, {
          fetches: __jtml_fetch_fns,
          fetchGroups: __jtml_fetch_groups,
          fetchTimers: __jtml_fetch_timers,
          resolveFetchUrl: resolveFetchUrl,
          resolveFetchKey: resolveFetchKey,
          refreshFetch: function (name) {
            if (__jtml_fetch_fns[name]) return __jtml_fetch_fns[name](true);
            return Promise.reject(new Error('Unknown JTML fetch: ' + name));
          }
        });
      }

      window.__jtml_redirect = function (path) {
        if (!path) return;
        const hash = path[0] === '#' ? path : '#' + (path[0] === '/' ? path : '/' + path);
        location.hash = hash;
      };

      function applyBindings(b) {
        if (!b) return;
        if (b.state) {
          for (const name in b.state) {
            clientState[name] = b.state[name];
          }
        }
        if (b.bindings) {
          for (const name in b.bindings) {
            clientState[name] = b.bindings[name];
          }
        }
        applyTemplates(b);
        if (b.content) {
          for (const id in b.content) {
            const el = document.getElementById(id);
            if (el) el.textContent = b.content[id];
          }
        }
        if (b.attributes) {
          for (const id in b.attributes) {
            const el = document.getElementById(id);
            if (!el) continue;
            for (const a in b.attributes[id]) {
              applyAttribute(el, a, b.attributes[id][a]);
            }
          }
        }
        applyClientExpressions();
        renderCharts();
        processImageBindings();
        applyRoutes();
      }

      function applyAttribute(el, attr, value) {
        el.setAttribute(attr, value);
          if (attr === 'value' && (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA')) {
          el.value = value;
        }
        if ((attr === 'disabled' || attr === 'checked' || attr === 'hidden' || attr === 'open') && !value) {
          el.removeAttribute(attr);
          if (attr in el) el[attr] = false;
        } else if ((attr === 'disabled' || attr === 'checked' || attr === 'hidden' || attr === 'open') && value) {
          el.setAttribute(attr, attr);
          if (attr in el) el[attr] = true;
        }
      }

      function renderTemplateValue(value) {
        if (value == null) return '';
        if (typeof value === 'object') return JSON.stringify(value);
        return String(value);
      }

      function escapeSvgText(value) {
        return String(value == null ? '' : value)
          .replace(/&/g, '&amp;')
          .replace(/</g, '&lt;')
          .replace(/>/g, '&gt;')
          .replace(/"/g, '&quot;');
      }

      function normalizeChartRows(value) {
        if (value && typeof value === 'object' && Array.isArray(value.data)) {
          value = value.data;
        }
        if (Array.isArray(value)) return value;
        if (value && typeof value === 'object') return Object.values(value);
        return [];
      }

      function sanitizeChartFileName(value) {
        return String(value || 'jtml-chart')
          .toLowerCase()
          .replace(/[^a-z0-9_-]+/g, '-')
          .replace(/^-+|-+$/g, '') || 'jtml-chart';
      }

      function chartDownloadName(svg, extension) {
        const label = svg.getAttribute('aria-label') || svg.getAttribute('data-jtml-chart') || 'jtml-chart';
        return sanitizeChartFileName(label) + '.' + extension;
      }

      function downloadChartBlob(filename, type, content) {
        const blob = content instanceof Blob ? content : new Blob([content], { type: type });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = filename;
        a.rel = 'noopener';
        document.body.appendChild(a);
        a.click();
        a.remove();
        setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
      }

      function serializeChartSvg(svg) {
        const clone = svg.cloneNode(true);
        clone.removeAttribute('data-jtml-chart-rendered');
        clone.setAttribute('xmlns', 'http://www.w3.org/2000/svg');
        return new XMLSerializer().serializeToString(clone);
      }

      function exportChartCsv(svg, rows, byField, series) {
        const escapeCsv = function (value) {
          const text = value == null ? '' : String(value);
          return /[",\n]/.test(text) ? '"' + text.replace(/"/g, '""') + '"' : text;
        };
        const headers = [byField].concat(series.map(function (s) { return s.field; }));
        const lines = [headers.map(escapeCsv).join(',')];
        rows.forEach(function (row, index) {
          const label = row && row[byField] != null ? row[byField] : String(index + 1);
          lines.push([label].concat(series.map(function (s) {
            return row && row[s.field] != null ? row[s.field] : '';
          })).map(escapeCsv).join(','));
        });
        downloadChartBlob(chartDownloadName(svg, 'csv'), 'text/csv;charset=utf-8', lines.join('\n') + '\n');
      }

      function exportChartPng(svg) {
        const source = serializeChartSvg(svg);
        const blob = new Blob([source], { type: 'image/svg+xml;charset=utf-8' });
        const url = URL.createObjectURL(blob);
        const img = new Image();
        img.onload = function () {
          const canvas = document.createElement('canvas');
          const width = Math.max(160, Number(svg.getAttribute('width') || 640) || 640);
          const height = Math.max(120, Number(svg.getAttribute('height') || 320) || 320);
          canvas.width = width;
          canvas.height = height;
          const ctx = canvas.getContext('2d');
          ctx.fillStyle = '#ffffff';
          ctx.fillRect(0, 0, width, height);
          ctx.drawImage(img, 0, 0, width, height);
          canvas.toBlob(function (pngBlob) {
            URL.revokeObjectURL(url);
            if (pngBlob) downloadChartBlob(chartDownloadName(svg, 'png'), 'image/png', pngBlob);
          }, 'image/png');
        };
        img.onerror = function () {
          URL.revokeObjectURL(url);
          reportStatus('error', 'Chart PNG export failed');
        };
        img.src = url;
      }

      function syncChartExportControls(svg, formats, rows, byField, series) {
        const marker = 'data-jtml-chart-export-controls';
        let controls = svg.nextElementSibling && svg.nextElementSibling.hasAttribute(marker)
          ? svg.nextElementSibling
          : null;
        if (!formats.length) {
          if (controls) controls.remove();
          return;
        }
        if (!controls) {
          controls = document.createElement('div');
          controls.setAttribute(marker, 'true');
          controls.style.cssText = 'display:flex;gap:8px;flex-wrap:wrap;margin:8px 0 16px;';
          svg.insertAdjacentElement('afterend', controls);
        }
        controls.innerHTML = '';
        formats.forEach(function (format) {
          const button = document.createElement('button');
          button.type = 'button';
          button.textContent = 'Export ' + format.toUpperCase();
          button.style.cssText = 'font:600 12px system-ui,sans-serif;border:1px solid #cbd5e1;border-radius:8px;background:#fff;color:#0f172a;padding:6px 10px;cursor:pointer;';
          button.addEventListener('click', function () {
            if (format === 'svg') {
              downloadChartBlob(chartDownloadName(svg, 'svg'), 'image/svg+xml;charset=utf-8', serializeChartSvg(svg));
            } else if (format === 'png') {
              exportChartPng(svg);
            } else if (format === 'csv') {
              exportChartCsv(svg, rows, byField, series);
            }
          });
          controls.appendChild(button);
        });
      }

      function renderCharts() {
        document.querySelectorAll('svg[data-jtml-chart]').forEach(function (svg) {
          const type = svg.getAttribute('data-jtml-chart') || 'bar';
          const dataExpr = svg.getAttribute('data-jtml-chart-data') || '';
          const byField = svg.getAttribute('data-jtml-chart-by') || 'label';
          const valueField = svg.getAttribute('data-jtml-chart-value') || 'value';
          const splitCsv = function (value) {
            return String(value || '').split(',').map(function (item) { return item.trim(); }).filter(Boolean);
          };
          const valueFields = splitCsv(svg.getAttribute('data-jtml-chart-values') || valueField || 'value');
          const seriesLabels = splitCsv(svg.getAttribute('data-jtml-chart-series') || '').map(function (label, index) {
            return label || valueFields[index] || ('Series ' + (index + 1));
          });
          const chartColors = splitCsv(svg.getAttribute('data-jtml-chart-colors') || '');
          const data = evaluateClientExpression(dataExpr);
          if (!data.found) return;
          const rows = normalizeChartRows(data.value);
          const width = Math.max(160, Number(svg.getAttribute('width') || 640) || 640);
          const height = Math.max(120, Number(svg.getAttribute('height') || 320) || 320);
          const axisXLabel = svg.getAttribute('data-jtml-chart-axis-x') || '';
          const axisYLabel = svg.getAttribute('data-jtml-chart-axis-y') || '';
          const showLegend = svg.getAttribute('data-jtml-chart-legend') === 'true';
          const showGrid   = svg.getAttribute('data-jtml-chart-grid') === 'true';
          const isStacked  = svg.getAttribute('data-jtml-chart-stacked') === 'true';
          const scaleMinAttr = svg.getAttribute('data-jtml-chart-min');
          const scaleMaxAttr = svg.getAttribute('data-jtml-chart-max');
          const tickCountAttr = svg.getAttribute('data-jtml-chart-ticks');
          const parseAnnotations = function (value) {
            return String(value || '').split(';').map(function (entry) {
              const parts = entry.split('|');
              return {
                label: (parts[0] || '').trim(),
                at: (parts[1] || '').trim(),
                field: (parts[2] || '').trim(),
                color: (parts[3] || '').trim()
              };
            }).filter(function (item) { return item.label && item.at; });
          };
          const annotations = parseAnnotations(svg.getAttribute('data-jtml-chart-annotations') || '');
          const exportFormats = splitCsv(svg.getAttribute('data-jtml-chart-export') || '').filter(function (format) {
            return format === 'svg' || format === 'png' || format === 'csv';
          });
          const padLeft  = axisYLabel ? 60 : 48;
          const padRight = showLegend ? 130 : 20;
          const padTop   = 20;
          const padBottom = axisXLabel ? 56 : 44;
          const pad = { left: padLeft, right: padRight, top: padTop, bottom: padBottom };
          const innerW = Math.max(1, width - pad.left - pad.right);
          const innerH = Math.max(1, height - pad.top - pad.bottom);
          const color = svg.getAttribute('data-jtml-chart-color') || '#0f766e';
          const palette = chartColors.length ? chartColors : [
            color, '#2563eb', '#b42318', '#9333ea', '#f59e0b', '#0891b2', '#16a34a'
          ];
          const series = valueFields.map(function (field, index) {
            return {
              field: field,
              label: seriesLabels[index] || field || ('Series ' + (index + 1)),
              color: palette[index % palette.length]
            };
          });
          const rowValues = rows.map(function (row) {
            return series.map(function (s) {
              return Number(row && row[s.field]) || 0;
            });
          });
          const values = rowValues.reduce(function (all, row) { return all.concat(row); }, []);
          const stackedValues = rowValues.map(function (row) {
            return row.reduce(function (sum, value) { return sum + value; }, 0);
          });
          const maxValue = Math.max(1, Math.max.apply(null, values.length ? values : [1]));
          const minValue = Math.min(0, Math.min.apply(null, values.length ? values : [0]));
          const explicitMin = scaleMinAttr !== null && scaleMinAttr !== '' && Number.isFinite(Number(scaleMinAttr))
            ? Number(scaleMinAttr)
            : minValue;
          const inferredMaxValue = type === 'bar' && isStacked
            ? Math.max(1, Math.max.apply(null, stackedValues.length ? stackedValues : [1]))
            : maxValue;
          const explicitMax = scaleMaxAttr !== null && scaleMaxAttr !== '' && Number.isFinite(Number(scaleMaxAttr))
            ? Number(scaleMaxAttr)
            : null;
          const scaleMaxValue = explicitMax !== null ? Math.max(explicitMin + 1, explicitMax) : inferredMaxValue;
          // Compute a clean tick interval
          const requestedTicks = Math.max(2, Math.min(10, Math.round(Number(tickCountAttr || 5) || 5)));
          const rawRange = Math.max(1, scaleMaxValue - explicitMin);
          const rawStep = rawRange / (requestedTicks - 1);
          const magnitude = Math.pow(10, Math.floor(Math.log10(rawStep || 1)));
          const niceStep = Math.ceil(rawStep / magnitude) * magnitude || 1;
          const niceMax = explicitMax !== null ? scaleMaxValue : (Math.ceil(scaleMaxValue / niceStep) * niceStep || niceStep);
          const niceMin = explicitMin;
          const niceRange = Math.max(1, niceMax - niceMin);
          const scaleRatio = function (value) {
            return Math.max(0, Math.min(1, (value - niceMin) / niceRange));
          };

          let html = '';
          // Grid lines and Y-axis ticks
          const tickCount = requestedTicks - 1;
          for (let t = 0; t <= tickCount; t++) {
            const tv = niceMin + t * (niceRange / Math.max(1, tickCount));
            const ty = (pad.top + innerH) - scaleRatio(tv) * innerH;
            if (showGrid && t > 0) {
              html += '<line x1="' + pad.left + '" y1="' + ty.toFixed(1) + '" x2="' + (width - pad.right) + '" y2="' + ty.toFixed(1) + '" stroke="#e2e8f0" stroke-width="1" stroke-dasharray="3,3"/>';
            }
            const tickLabel = tv >= 1000 ? (tv / 1000).toFixed(1) + 'k' : String(tv);
            html += '<line x1="' + (pad.left - 4) + '" y1="' + ty.toFixed(1) + '" x2="' + pad.left + '" y2="' + ty.toFixed(1) + '" stroke="#94a3b8" stroke-width="1"/>';
            html += '<text x="' + (pad.left - 7) + '" y="' + (ty + 4).toFixed(1) + '" text-anchor="end" fill="#64748b" font-size="11">' + escapeSvgText(tickLabel) + '</text>';
          }
          // Axes
          html += '<line x1="' + pad.left + '" y1="' + (height - pad.bottom) + '" x2="' + (width - pad.right) + '" y2="' + (height - pad.bottom) + '" stroke="#94a3b8" stroke-width="1"/>';
          html += '<line x1="' + pad.left + '" y1="' + pad.top + '" x2="' + pad.left + '" y2="' + (height - pad.bottom) + '" stroke="#94a3b8" stroke-width="1"/>';
          // Y-axis label
          if (axisYLabel) {
            var rotTransform = 'rotate(-90)';
            html += '<text transform="' + rotTransform + '" x="' + (-(height / 2)).toFixed(1) + '" y="14" text-anchor="middle" fill="#475569" font-size="12">' + escapeSvgText(axisYLabel) + '</text>';
          }
          // X-axis label
          if (axisXLabel) {
            html += '<text x="' + ((pad.left + width - pad.right) / 2).toFixed(1) + '" y="' + (height - 6) + '" text-anchor="middle" fill="#475569" font-size="12">' + escapeSvgText(axisXLabel) + '</text>';
          }
          if (!rows.length) {
            html += '<text x="' + (width / 2) + '" y="' + (height / 2) + '" text-anchor="middle" fill="#64748b" font-size="14">No chart data</text>';
          }
          if (type === 'bar') {
            const groupGap = rows.length > 1 ? Math.max(6, Math.round(innerW / Math.max(1, rows.length) * 0.14)) : 0;
            const groupW = rows.length ? Math.max(2, (innerW - groupGap * (rows.length - 1)) / rows.length) : innerW;
            rows.forEach(function (row, index) {
              const label = row && row[byField] != null ? row[byField] : String(index + 1);
              const xGroup = pad.left + index * (groupW + groupGap);
              const xLabel = xGroup + groupW / 2;
              if (isStacked) {
                let stackedY = height - pad.bottom;
                rowValues[index].forEach(function (value, seriesIndex) {
                  const barH = Math.round(scaleRatio(value) * innerH);
                  stackedY -= barH;
                  const barW = Math.max(2, groupW * 0.72);
                  const x = xGroup + (groupW - barW) / 2;
                  html += '<rect x="' + x.toFixed(2) + '" y="' + stackedY.toFixed(2) + '" width="' + barW.toFixed(2) + '" height="' + barH.toFixed(2) + '" fill="' + escapeSvgText(series[seriesIndex].color) + '" rx="2"/>';
                });
              } else {
                const barGap = series.length > 1 ? 2 : 0;
                const barW = Math.max(2, (groupW - barGap * (series.length - 1)) / Math.max(1, series.length));
                rowValues[index].forEach(function (value, seriesIndex) {
                  const barH = Math.round(scaleRatio(value) * innerH);
                  const x = xGroup + seriesIndex * (barW + barGap);
                  const y = height - pad.bottom - barH;
                  html += '<rect x="' + x.toFixed(2) + '" y="' + y.toFixed(2) + '" width="' + barW.toFixed(2) + '" height="' + barH.toFixed(2) + '" fill="' + escapeSvgText(series[seriesIndex].color) + '" rx="2"/>';
                  if (series.length === 1 && barH > 14) {
                    html += '<text x="' + (x + barW / 2).toFixed(2) + '" y="' + Math.max(pad.top + 12, y - 4).toFixed(2) + '" text-anchor="middle" fill="#334155" font-size="11">' + escapeSvgText(value) + '</text>';
                  }
                });
              }
              const truncLabel = String(label).length > 8 ? String(label).slice(0, 7) + '…' : String(label);
              html += '<text x="' + xLabel.toFixed(2) + '" y="' + (height - pad.bottom + 14) + '" text-anchor="middle" fill="#334155" font-size="11">' + escapeSvgText(truncLabel) + '</text>';
            });
          } else if (type === 'line') {
            const stepX = rows.length > 1 ? innerW / (rows.length - 1) : innerW;
            series.forEach(function (s, seriesIndex) {
              let points = '';
              rows.forEach(function (row, index) {
                const value = rowValues[index][seriesIndex];
                const x = (pad.left + (rows.length > 1 ? index * stepX : innerW / 2)).toFixed(2);
                const y = (height - pad.bottom - scaleRatio(value) * innerH).toFixed(2);
                if (index === 0) points += 'M' + x + ' ' + y;
                else points += ' L' + x + ' ' + y;
              });
              if (rows.length > 1 && series.length === 1) {
                const firstX = pad.left.toFixed(2);
                const lastX  = (pad.left + innerW).toFixed(2);
                const baseY  = (height - pad.bottom).toFixed(2);
                html += '<path d="' + points + ' L' + lastX + ' ' + baseY + ' L' + firstX + ' ' + baseY + ' Z" fill="' + escapeSvgText(s.color) + '" fill-opacity="0.12"/>';
              }
              html += '<path d="' + points + '" fill="none" stroke="' + escapeSvgText(s.color) + '" stroke-width="2.5" stroke-linejoin="round" stroke-linecap="round"/>';
              rows.forEach(function (row, index) {
                const value = rowValues[index][seriesIndex];
                const x = (pad.left + (rows.length > 1 ? index * stepX : innerW / 2)).toFixed(2);
                const y = (height - pad.bottom - scaleRatio(value) * innerH).toFixed(2);
                html += '<circle cx="' + x + '" cy="' + y + '" r="4" fill="' + escapeSvgText(s.color) + '" stroke="#fff" stroke-width="1.5"/>';
              });
            });
            rows.forEach(function (row, index) {
              if (rows.length <= 16) {
                const x = (pad.left + (rows.length > 1 ? index * stepX : innerW / 2)).toFixed(2);
                const label = row && row[byField] != null ? row[byField] : String(index + 1);
                const truncLabel = String(label).length > 8 ? String(label).slice(0, 7) + '…' : String(label);
                html += '<text x="' + x + '" y="' + (height - pad.bottom + 14) + '" text-anchor="middle" fill="#334155" font-size="11">' + escapeSvgText(truncLabel) + '</text>';
              }
            });
          }
          if (annotations.length && rows.length) {
            const lineStepX = rows.length > 1 ? innerW / (rows.length - 1) : innerW;
            const barGroupGap = rows.length > 1 ? Math.max(6, Math.round(innerW / Math.max(1, rows.length) * 0.14)) : 0;
            const barGroupW = rows.length ? Math.max(2, (innerW - barGroupGap * (rows.length - 1)) / rows.length) : innerW;
            annotations.forEach(function (annotation) {
              const rowIndex = rows.findIndex(function (row) {
                return String(row && row[byField] != null ? row[byField] : '') === annotation.at;
              });
              if (rowIndex < 0) return;
              const seriesIndex = Math.max(0, series.findIndex(function (s) {
                return s.field === annotation.field || s.label === annotation.field;
              }));
              const value = rowValues[rowIndex] && rowValues[rowIndex][seriesIndex] != null
                ? rowValues[rowIndex][seriesIndex]
                : 0;
              const x = type === 'bar'
                ? pad.left + rowIndex * (barGroupW + barGroupGap) + barGroupW / 2
                : pad.left + (rows.length > 1 ? rowIndex * lineStepX : innerW / 2);
              const y = height - pad.bottom - scaleRatio(value) * innerH;
              const noteColor = annotation.color || (series[seriesIndex] && series[seriesIndex].color) || '#111827';
              const labelY = Math.max(pad.top + 12, y - 12);
              html += '<line x1="' + x.toFixed(2) + '" y1="' + pad.top + '" x2="' + x.toFixed(2) + '" y2="' + (height - pad.bottom) + '" stroke="' + escapeSvgText(noteColor) + '" stroke-width="1" stroke-dasharray="4,4" opacity="0.72"/>';
              html += '<circle cx="' + x.toFixed(2) + '" cy="' + y.toFixed(2) + '" r="4.5" fill="' + escapeSvgText(noteColor) + '" stroke="#fff" stroke-width="1.5"/>';
              html += '<text x="' + Math.min(width - pad.right - 4, x + 8).toFixed(2) + '" y="' + labelY.toFixed(2) + '" fill="' + escapeSvgText(noteColor) + '" font-size="11" font-weight="600">' + escapeSvgText(annotation.label) + '</text>';
            });
          }
          // Legend
          if (showLegend && rows.length > 0) {
            const lx = width - pad.right + 12;
            let ly = pad.top + 10;
            series.forEach(function (s) {
              html += '<rect x="' + lx + '" y="' + ly + '" width="12" height="12" fill="' + escapeSvgText(s.color) + '" rx="2"/>';
              html += '<text x="' + (lx + 16) + '" y="' + (ly + 10) + '" fill="#334155" font-size="12">' + escapeSvgText(s.label) + '</text>';
              ly += 18;
            });
          }
          if (svg.dataset.jtmlChartRendered !== html) {
            svg.innerHTML = html;
            svg.dataset.jtmlChartRendered = html;
          }
          syncChartExportControls(svg, exportFormats, rows, byField, series);
        });
      }

      // ── Timeline animations ────────────────────────────────────────
      const __jtml_timelines = {};
      function initTimelines() {
        document.querySelectorAll('template[data-jtml-timeline]').forEach(function (el) {
          const name = el.getAttribute('data-jtml-timeline');
          if (!name || __jtml_timelines[name]) return;
          const duration = Math.max(1, Number(el.getAttribute('data-jtml-timeline-duration')) || 400);
          const easingName = el.getAttribute('data-jtml-timeline-easing') || 'linear';
          const autoplay = el.getAttribute('data-jtml-timeline-autoplay') === 'true';
          const repeat = el.getAttribute('data-jtml-timeline-repeat') === 'true';
          const animateStr = el.getAttribute('data-jtml-timeline-animates') || '';
          const animates = animateStr.split(';').filter(Boolean).map(function (s) {
            const parts = s.split(':');
            return { varName: parts[0], from: Number(parts[1]) || 0, to: Number(parts[2]) || 0 };
          });
          function easeValue(t, easingFn) {
            if (easingFn === 'ease-in')     return t * t;
            if (easingFn === 'ease-out')    return 1 - (1 - t) * (1 - t);
            if (easingFn === 'ease-in-out') return t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2;
            if (easingFn === 'cubic-bezier') return t; // fallback
            return t; // linear
          }
          const tl = {
            name: name, duration: duration, easing: easingName,
            playing: false, paused: false, elapsed: 0, startTime: null, rafId: null, repeat: repeat,
            play: function () {
              if (tl.playing && !tl.paused) return;
              if (tl.paused) { tl.startTime = performance.now() - tl.elapsed; tl.paused = false; }
              else { tl.startTime = performance.now(); tl.elapsed = 0; }
              tl.playing = true;
              __jtml_sendEvent('__timeline_' + name + '_play', []);
              function tick(now) {
                if (!tl.playing || tl.paused) return;
                tl.elapsed = now - tl.startTime;
                const rawT = Math.min(1, tl.elapsed / tl.duration);
                const t = easeValue(rawT, easingName);
                const progress = Math.round(t * 100);
                animates.forEach(function (a) {
                  const val = a.from + (a.to - a.from) * t;
                  __jtml_sendEvent('__timeline_animate_' + a.varName, [val]);
                });
                __jtml_sendEvent('__timeline_' + name + '_progress', [progress]);
                if (rawT < 1) { tl.rafId = requestAnimationFrame(tick); }
                else {
                  tl.playing = false;
                  if (repeat) { tl.elapsed = 0; tl.startTime = now; tl.playing = true; tl.rafId = requestAnimationFrame(tick); }
                  else { __jtml_sendEvent('__timeline_' + name + '_done', []); }
                }
              }
              tl.rafId = requestAnimationFrame(tick);
            },
            pause: function () {
              if (!tl.playing) return;
              tl.paused = true; tl.playing = false;
              if (tl.rafId) cancelAnimationFrame(tl.rafId);
              __jtml_sendEvent('__timeline_' + name + '_pause', []);
            },
            reset: function () {
              tl.playing = false; tl.paused = false; tl.elapsed = 0;
              if (tl.rafId) cancelAnimationFrame(tl.rafId);
              animates.forEach(function (a) {
                __jtml_sendEvent('__timeline_animate_' + a.varName, [a.from]);
              });
              __jtml_sendEvent('__timeline_' + name + '_progress', [0]);
            }
          };
          __jtml_timelines[name] = tl;
          if (autoplay) setTimeout(function () { tl.play(); }, 50);
        });
      }

      function __jtml_sendEvent(action, args) {
        try {
          sendEvent(action, args);
        } catch (e) {}
      }

      // ── Browser image processing ───────────────────────────────────
      function processImageBindings() {
        document.querySelectorAll('template[data-jtml-image-proc]').forEach(function (el) {
          const op     = el.getAttribute('data-jtml-image-proc') || 'resize';
          const srcVar = el.getAttribute('data-jtml-image-src') || '';
          const into   = el.getAttribute('data-jtml-image-into') || '';
          if (!srcVar || !into) return;
          const srcData = evaluateClientExpression(srcVar);
          if (!srcData.found) return;
          const srcValue = srcData.value;
          const srcUrl = typeof srcValue === 'object' && srcValue
            ? (srcValue.preview || srcValue.url || srcValue.src || '')
            : String(srcValue || '');
          if (!srcUrl) return;
          const cacheKey = op + ':' + srcVar + ':' + JSON.stringify({
            w: el.getAttribute('data-jtml-image-w'),
            h: el.getAttribute('data-jtml-image-h'),
            fit: el.getAttribute('data-jtml-image-fit'),
            x: el.getAttribute('data-jtml-image-x'),
            y: el.getAttribute('data-jtml-image-y'),
            filter: el.getAttribute('data-jtml-image-filter'),
            amount: el.getAttribute('data-jtml-image-amount'),
            src: srcUrl
          });
          if (el.dataset.jtmlImgCacheKey === cacheKey) return;
          el.dataset.jtmlImgCacheKey = cacheKey;
          const img = new Image();
          img.crossOrigin = 'anonymous';
          img.onload = function () {
            try {
              const canvas = document.createElement('canvas');
              let sw = img.naturalWidth, sh = img.naturalHeight;
              let dx = 0, dy = 0, dw, dh;
              if (op === 'resize') {
                const tw = Number(el.getAttribute('data-jtml-image-w')) || sw;
                const th = Number(el.getAttribute('data-jtml-image-h')) || sh;
                const fit = el.getAttribute('data-jtml-image-fit') || 'fill';
                const ratio = sw / sh;
                if (fit === 'cover') {
                  if (tw / th > ratio) { dw = tw; dh = Math.round(tw / ratio); }
                  else { dh = th; dw = Math.round(th * ratio); }
                  dx = Math.round((dw - tw) / 2); dy = Math.round((dh - th) / 2);
                  canvas.width = tw; canvas.height = th;
                  canvas.getContext('2d').drawImage(img, -dx, -dy, dw, dh);
                } else if (fit === 'contain') {
                  if (tw / th > ratio) { dh = th; dw = Math.round(th * ratio); }
                  else { dw = tw; dh = Math.round(tw / ratio); }
                  canvas.width = tw; canvas.height = th;
                  canvas.getContext('2d').drawImage(img, Math.round((tw - dw) / 2), Math.round((th - dh) / 2), dw, dh);
                } else {
                  canvas.width = tw; canvas.height = th;
                  canvas.getContext('2d').drawImage(img, 0, 0, tw, th);
                }
              } else if (op === 'crop') {
                const cx = Number(el.getAttribute('data-jtml-image-x')) || 0;
                const cy = Number(el.getAttribute('data-jtml-image-y')) || 0;
                const cw = Number(el.getAttribute('data-jtml-image-w')) || sw;
                const ch = Number(el.getAttribute('data-jtml-image-h')) || sh;
                canvas.width = cw; canvas.height = ch;
                canvas.getContext('2d').drawImage(img, cx, cy, cw, ch, 0, 0, cw, ch);
              } else if (op === 'filter') {
                const filterType = el.getAttribute('data-jtml-image-filter') || '';
                const amount = Number(el.getAttribute('data-jtml-image-amount') || '1');
                canvas.width = sw; canvas.height = sh;
                const ctx = canvas.getContext('2d');
                if (filterType === 'grayscale') {
                  ctx.filter = 'grayscale(' + (amount * 100).toFixed(0) + '%)';
                } else if (filterType === 'blur') {
                  ctx.filter = 'blur(' + amount.toFixed(1) + 'px)';
                } else if (filterType === 'brightness') {
                  ctx.filter = 'brightness(' + amount.toFixed(2) + ')';
                } else if (filterType === 'contrast') {
                  ctx.filter = 'contrast(' + amount.toFixed(2) + ')';
                } else if (filterType === 'sepia') {
                  ctx.filter = 'sepia(' + (amount * 100).toFixed(0) + '%)';
                } else if (filterType === 'invert') {
                  ctx.filter = 'invert(' + (amount * 100).toFixed(0) + '%)';
                } else if (filterType === 'saturate') {
                  ctx.filter = 'saturate(' + amount.toFixed(2) + ')';
                }
                ctx.drawImage(img, 0, 0, sw, sh);
              }
              const preview = canvas.toDataURL('image/png');
              assignClientPath(into, { preview: preview, loading: false, error: '', width: canvas.width, height: canvas.height });
              applyClientState();
            } catch (err) {
              assignClientPath(into, { preview: '', loading: false, error: String(err), width: 0, height: 0 });
              applyClientState();
            }
          };
          img.onerror = function () {
            assignClientPath(into, { preview: '', loading: false, error: 'Failed to load image', width: 0, height: 0 });
            applyClientState();
          };
          img.src = srcUrl;
        });
      }

      function lookupTemplatePath(scope, path) {
        const parts = path.split('.').filter(Boolean);
        let value = scope;
        for (const part of parts) {
          if (value == null) return '';
          value = value[part];
        }
        return renderTemplateValue(value);
      }

      function applyScopedExpressions(root, scope) {
        root.querySelectorAll('[data-jtml-expr]').forEach(function (el) {
          const result = evaluateClientExpression(el.getAttribute('data-jtml-expr'), scope);
          if (result.found) el.textContent = renderTemplateValue(result.value);
        });
      }

      function applyScopedAttributes(root, scope) {
        root.querySelectorAll('*').forEach(function (el) {
          Array.prototype.slice.call(el.attributes || []).forEach(function (attr) {
            const match = attr.name.match(/^data-jtml-attr-(.+)-expr$/);
            if (!match) return;
            const result = evaluateClientExpression(attr.value, scope);
            if (result.found) applyAttribute(el, match[1], result.value);
          });
        });
      }

      function applyScopedTemplates(root, scope) {
        for (let pass = 0; pass < 5; pass += 1) {
          let changed = false;
          root.querySelectorAll('[data-jtml-cond-expr]').forEach(function (el) {
            const result = evaluateClientExpression(el.getAttribute('data-jtml-cond-expr'), scope);
            if (!result.found) return;
            const source = result.value ? el.getAttribute('data-then') || el.getAttribute('data-body') || '' : el.getAttribute('data-else') || '';
            if (el.dataset.jtmlRendered !== source) {
              el.innerHTML = renderLoopBody(source, '', null, scope);
              el.dataset.jtmlRendered = source;
              changed = true;
            }
          });
          root.querySelectorAll('[data-jtml-for-expr]').forEach(function (el) {
            const result = evaluateClientExpression(el.getAttribute('data-jtml-for-expr'), scope);
            if (!result.found) return;
            const iterator = el.getAttribute('data-jtml-iterator') || 'item';
            const body = el.getAttribute('data-body') || '';
            let values = result.value;
            if (values == null) values = [];
            if (!Array.isArray(values)) {
              if (typeof values === 'string') values = values.split('');
              else if (typeof values === 'object') values = Object.values(values);
              else values = [values];
            }
            const html = values.map(function (nestedItem) {
              return renderLoopBody(body, iterator, nestedItem, scope);
            }).join('');
            if (el.dataset.jtmlRendered !== html) {
              el.innerHTML = html;
              el.dataset.jtmlRendered = html;
              changed = true;
            }
          });
          applyScopedExpressions(root, scope);
          applyScopedAttributes(root, scope);
          if (!changed) break;
        }
      }

      function escapeSelectorValue(value) {
        if (window.CSS && typeof window.CSS.escape === 'function') return window.CSS.escape(value);
        return String(value).replace(/["\\]/g, '\\$&');
      }

      function renderLoopBody(template, iterator, item, parentScope) {
        const scope = Object.assign({}, parentScope || {});
        if (iterator) scope[iterator] = item;
        const substituted = String(template || '').replace(/\{\{\s*\(?\s*([A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*)\s*\)?\s*\}\}/g,
          function (_, expr) { return lookupTemplatePath(scope, expr); });
        const holder = document.createElement('template');
        holder.innerHTML = substituted;
        applyScopedTemplates(holder.content, scope);
        applyScopedExpressions(holder.content, scope);
        applyScopedAttributes(holder.content, scope);
        return holder.innerHTML;
      }

      function applyTemplates(b) {
        if (b.conditions) {
          for (const key in b.conditions) {
            const escapedKey = escapeSelectorValue(key);
            document.querySelectorAll('[data-jtml-if="' + escapedKey + '"], [data-jtml-while="' + escapedKey + '"]').forEach(function (el) {
              const source = b.conditions[key] ? el.getAttribute('data-then') || el.getAttribute('data-body') || '' : el.getAttribute('data-else') || '';
              if (el.dataset.jtmlRendered !== source) {
                el.innerHTML = source;
                el.dataset.jtmlRendered = source;
              }
            });
          }
        }
        if (b.loops) {
          for (const id in b.loops) {
            const el = document.getElementById(id);
            if (!el) continue;
            const iterator = el.getAttribute('data-jtml-iterator') || 'item';
            const body = el.getAttribute('data-body') || '';
            let values = b.loops[id];
            if (values == null) values = [];
            if (!Array.isArray(values)) {
              if (typeof values === 'string') values = values.split('');
              else if (typeof values === 'object') values = Object.values(values);
              else values = [values];
            }
            const html = values.map(function (item) { return renderLoopBody(body, iterator, item); }).join('');
            if (el.dataset.jtmlRendered !== html) {
              el.innerHTML = html;
              el.dataset.jtmlRendered = html;
            }
          }
        }
      }

      function startRedirectBindings() {
        document.querySelectorAll('[data-jtml-redirect-action]').forEach(function (meta) {
          const actionName = meta.getAttribute('data-jtml-redirect-action');
          const path = meta.getAttribute('data-jtml-redirect-to') || '/';
          if (actionName) {
            __jtml_refresh_fns[actionName] = function () {
              window.__jtml_redirect(path);
            };
          }
        });
      }

      function startInvalidationBindings() {
        document.querySelectorAll('[data-jtml-invalidate-action]').forEach(function (meta) {
          const actionName = meta.getAttribute('data-jtml-invalidate-action');
          const fetches = String(meta.getAttribute('data-jtml-invalidate-fetches') || '')
            .split(',')
            .map(function (name) { return name.trim(); })
            .filter(Boolean);
          const groups = String(meta.getAttribute('data-jtml-invalidate-groups') || '')
            .split(',')
            .map(function (name) { return name.trim(); })
            .filter(Boolean);
          const all = meta.getAttribute('data-jtml-invalidate-all') === 'true';
          if (actionName && (fetches.length || groups.length || all)) {
            __jtml_invalidate_fns[actionName] = { fetches: fetches, groups: groups, all: all };
          }
        });
      }

      async function runInvalidations(actionName) {
        const spec = __jtml_invalidate_fns[actionName] || {};
        const names = new Set(Array.isArray(spec) ? spec : (spec.fetches || []));
        (spec.groups || []).forEach(function (group) {
          (__jtml_fetch_groups[group] || []).forEach(function (name) { names.add(name); });
        });
        if (spec.all) {
          Object.keys(__jtml_fetch_fns).forEach(function (name) { names.add(name); });
        }
        for (const name of names) {
          if (__jtml_fetch_fns[name]) await __jtml_fetch_fns[name](true);
        }
      }

      function runRouteLoads(route) {
        const fetches = String(route.getAttribute('data-jtml-route-load') || '')
          .split(',')
          .map(function (name) { return name.trim(); })
          .filter(Boolean);
        fetches.forEach(function (name) {
          if (__jtml_fetch_fns[name]) __jtml_fetch_fns[name]();
        });
      }

      const __jtml_routes = [];
      let __jtml_current_route = null;

      function publicRouteRecord(record) {
        return {
          path: record.path,
          name: record.name,
          params: record.params.slice(),
          load: record.load.slice()
        };
      }

      function publishRouteApi() {
        window.jtml = Object.assign(window.jtml || {}, {
          routes: __jtml_routes.map(publicRouteRecord),
          currentRoute: __jtml_current_route,
          getRoutes: function () {
            return __jtml_routes.map(publicRouteRecord);
          },
          getCurrentRoute: function () {
            return __jtml_current_route ? Object.assign({}, __jtml_current_route) : null;
          },
          navigate: function (path) {
            const before = location.hash;
            window.__jtml_redirect(path);
            if (location.hash === before) applyRoutes();
          }
        });
      }

      function collectRouteBindings() {
        __jtml_routes.length = 0;
        const routeElements = Array.prototype.slice.call(document.querySelectorAll('[data-jtml-route]'));
        const manifestRoutes = (window.jtml && Array.isArray(window.jtml.routeManifest))
          ? window.jtml.routeManifest
          : [];
        if (manifestRoutes.length) {
          manifestRoutes.forEach(function (manifest, index) {
            const path = manifest && manifest.path ? manifest.path : '/';
            const name = manifest && manifest.name ? manifest.name : '';
            const el = routeElements.find(function (candidate) {
              return candidate.getAttribute('data-jtml-route') === path &&
                (!name || candidate.getAttribute('data-jtml-route-name') === name);
            }) || routeElements[index];
            if (!el) return;
            __jtml_routes.push({
              element: el,
              path: path,
              name: name,
              params: Array.isArray(manifest.params) ? manifest.params.slice() : [],
              load: Array.isArray(manifest.load) ? manifest.load.slice() : []
            });
          });
        } else {
          routeElements.forEach(function (el) {
          __jtml_routes.push({
            element: el,
            path: el.getAttribute('data-jtml-route') || '/',
            name: el.getAttribute('data-jtml-route-name') || '',
            params: (el.getAttribute('data-jtml-route-params') || '').split(',').filter(Boolean),
            load: String(el.getAttribute('data-jtml-route-load') || '')
              .split(',')
              .map(function (name) { return name.trim(); })
              .filter(Boolean)
          });
        });
        }
        publishRouteApi();
        document.dispatchEvent(new CustomEvent('jtml:routes-ready', {
          detail: { routes: __jtml_routes.map(publicRouteRecord) }
        }));
        return __jtml_routes;
      }

      function getWindowPath(path) {
        return String(path || '').split('.').filter(Boolean).reduce(function (value, part) {
          return value == null ? undefined : value[part];
        }, window);
      }

      function parseActionCall(callSource) {
        const raw = String(callSource || '').trim();
        const match = raw.match(/^([A-Za-z_][A-Za-z0-9_.]*)(?:\((.*)\))?$/);
        if (!match) return { name: raw.replace(/\(.*$/, ''), args: [] };
        const args = [];
        const inner = (match[2] || '').trim();
        if (inner) {
          splitTopLevelList(inner).forEach(function (part) {
            const value = evaluateClientBodyExpression(part);
            args.push(value.found ? value.value : part.replace(/^['"]|['"]$/g, ''));
          });
        }
        return { name: match[1], args: args };
      }

      function startExternBindings() {
        document.querySelectorAll('[data-jtml-extern-action]').forEach(function (meta) {
          const action = meta.getAttribute('data-jtml-extern-action');
          const target = meta.getAttribute('data-window') || action;
          if (!action) return;
          __jtml_extern_fns[action] = function (args) {
            const fn = getWindowPath(target);
            if (typeof fn !== 'function') {
              reportStatus('error', 'External host function not found: ' + target);
              return;
            }
            return fn.apply(window, args || []);
          };
        });
      }

      // Intercept clicks on Friendly route links (`link "Label" to "/path"` →
      // an inert anchor carrying `data-jtml-href="#/path"`.
      // Older output used plain hash links, which could resolve against the
      // parent Studio URL inside `srcdoc` previews if the default navigation
      // ran before the router. Setting `location.hash` here mutates the
      // iframe's own Location and keeps the preview on the rendered JTML app.
      function startLinkBindings() {
        document.addEventListener('click', function (event) {
          if (event.defaultPrevented) return;
          if (event.button !== 0) return;
          if (event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return;
          const anchor = event.target && event.target.closest && event.target.closest('a[data-jtml-link]');
          if (!anchor) return;
          if (anchor.target && anchor.target !== '' && anchor.target !== '_self') return;
          const href = anchor.getAttribute('data-jtml-href') || anchor.getAttribute('href') || '';
          if (!href || href[0] !== '#') return;
          event.preventDefault();
          const next = href.length > 1 ? href : '#/';
          if (location.hash === next) {
            // Same-hash click still has to re-run route bindings so users
            // can re-trigger active-link feedback or load handlers.
            applyRoutes();
          } else {
            location.hash = next;
          }
        });
      }

      function startDropzoneBindings() {
        document.querySelectorAll('input[type="file"][data-jtml-dropzone]').forEach(function (input) {
          if (input.dataset.jtmlDropzoneReady === 'true') return;
          input.dataset.jtmlDropzoneReady = 'true';
          input.addEventListener('dragover', function (event) {
            event.preventDefault();
            input.dataset.jtmlDrag = 'over';
          });
          input.addEventListener('dragleave', function () {
            delete input.dataset.jtmlDrag;
          });
          input.addEventListener('drop', function (event) {
            event.preventDefault();
            delete input.dataset.jtmlDrag;
            const files = event.dataTransfer && event.dataTransfer.files;
            if (!files || !files.length) return;
            try {
              input.files = files;
            } catch (_) {}
            input.dispatchEvent(new Event('change', { bubbles: true }));
          });
        });
      }

      function mediaStateFor(el) {
        return {
          currentTime: Number.isFinite(el.currentTime) ? el.currentTime : 0,
          duration: Number.isFinite(el.duration) ? el.duration : 0,
          paused: !!el.paused,
          ended: !!el.ended,
          muted: !!el.muted,
          volume: Number.isFinite(el.volume) ? el.volume : 1,
          playbackRate: Number.isFinite(el.playbackRate) ? el.playbackRate : 1,
          readyState: Number(el.readyState || 0),
          src: el.currentSrc || el.getAttribute('src') || ''
        };
      }

      function startMediaControllerBindings() {
        document.querySelectorAll('video[data-jtml-media-controller], audio[data-jtml-media-controller]').forEach(function (el) {
          const name = el.getAttribute('data-jtml-media-controller');
          if (!name) return;
          const update = function () {
            clientState[name] = mediaStateFor(el);
            applyClientState();
          };
          if (el.dataset.jtmlMediaReady !== 'true') {
            el.dataset.jtmlMediaReady = 'true';
            ['loadedmetadata', 'durationchange', 'timeupdate', 'play', 'pause', 'ended', 'volumechange', 'ratechange', 'emptied'].forEach(function (eventName) {
              el.addEventListener(eventName, update);
            });
          }
          __jtml_media_actions[name + '.play'] = function () {
            const result = el.play && el.play();
            if (result && typeof result.catch === 'function') {
              result.catch(function (err) { reportStatus('error', err && err.message ? err.message : 'media play failed'); });
            }
            update();
          };
          __jtml_media_actions[name + '.pause'] = function () {
            if (el.pause) el.pause();
            update();
          };
          __jtml_media_actions[name + '.toggle'] = function () {
            if (el.paused) __jtml_media_actions[name + '.play']();
            else __jtml_media_actions[name + '.pause']();
          };
          __jtml_media_actions[name + '.seek'] = function (args) {
            const value = Number(args && args.length ? args[0] : 0);
            if (Number.isFinite(value)) el.currentTime = value;
            update();
          };
          __jtml_media_actions[name + '.setVolume'] = function (args) {
            const value = Number(args && args.length ? args[0] : el.volume);
            if (Number.isFinite(value)) el.volume = Math.max(0, Math.min(1, value));
            update();
          };
          update();
        });
      }

      function drawScene3DFallback(canvas, spec) {
        const ctx = canvas.getContext && canvas.getContext('2d');
        if (!ctx) return;
        const width = Math.max(240, Number(canvas.getAttribute('width') || canvas.clientWidth || 640) || 640);
        const height = Math.max(160, Number(canvas.getAttribute('height') || canvas.clientHeight || 360) || 360);
        canvas.width = width;
        canvas.height = height;
        ctx.clearRect(0, 0, width, height);
        const gradient = ctx.createLinearGradient(0, 0, width, height);
        gradient.addColorStop(0, '#0f172a');
        gradient.addColorStop(1, '#134e4a');
        ctx.fillStyle = gradient;
        ctx.fillRect(0, 0, width, height);
        const size = Math.min(width, height) * 0.28;
        const cx = width / 2;
        const cy = height / 2;
        const dx = size * 0.42;
        const dy = -size * 0.32;
        const front = [
          [cx - size / 2, cy - size / 2],
          [cx + size / 2, cy - size / 2],
          [cx + size / 2, cy + size / 2],
          [cx - size / 2, cy + size / 2]
        ];
        const back = front.map(function (p) { return [p[0] + dx, p[1] + dy]; });
        ctx.strokeStyle = '#7dd3fc';
        ctx.lineWidth = 2;
        const line = function (a, b) {
          ctx.beginPath();
          ctx.moveTo(a[0], a[1]);
          ctx.lineTo(b[0], b[1]);
          ctx.stroke();
        };
        for (let i = 0; i < 4; i += 1) {
          line(front[i], front[(i + 1) % 4]);
          line(back[i], back[(i + 1) % 4]);
          line(front[i], back[i]);
        }
        ctx.fillStyle = 'rgba(255,255,255,.92)';
        ctx.font = '600 14px system-ui, sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText(spec.scene ? '3D scene: ' + spec.scene : 'JTML 3D scene mount', cx, height - 42);
        ctx.font = '12px system-ui, sans-serif';
        ctx.fillStyle = 'rgba(226,232,240,.82)';
        ctx.fillText('Attach window.jtml3d.render(canvas, spec) for Three.js/WebGPU rendering', cx, height - 22);
      }

      function scene3DStateFor(canvas, spec, hostRendered, status, extra) {
        return Object.assign({
          scene: spec.scene || '',
          camera: spec.camera || 'orbit',
          controls: spec.controls || 'orbit',
          renderer: spec.renderer || 'auto',
          status: status || 'ready',
          hostRendered: !!hostRendered,
          width: Number(canvas.width || canvas.getAttribute('width') || canvas.clientWidth || 0),
          height: Number(canvas.height || canvas.getAttribute('height') || canvas.clientHeight || 0)
        }, extra && typeof extra === 'object' ? extra : {});
      }

      function startScene3DBindings() {
        document.querySelectorAll('canvas[data-jtml-scene3d]').forEach(function (canvas) {
          const controllerName = canvas.getAttribute('data-jtml-scene3d-controller') || '';
          const spec = {
            scene: canvas.getAttribute('data-jtml-scene') || '',
            camera: canvas.getAttribute('data-jtml-camera') || 'orbit',
            controls: canvas.getAttribute('data-jtml-controls') || 'orbit',
            renderer: canvas.getAttribute('data-jtml-renderer') || 'auto',
            controller: controllerName,
            state: clientState
          };
          const publish = function (extra, status, hostRendered) {
            if (!controllerName) return;
            clientState[controllerName] = scene3DStateFor(canvas, spec, hostRendered, status, extra);
            applyClientState();
          };
          spec.update = function (next) {
            publish(next, 'host-updated', true);
          };
          window.jtml = Object.assign(window.jtml || {}, {
            getScene3DSpec: function (el) {
              el = el || canvas;
              return {
                scene: el.getAttribute('data-jtml-scene') || '',
                camera: el.getAttribute('data-jtml-camera') || 'orbit',
                controls: el.getAttribute('data-jtml-controls') || 'orbit',
                renderer: el.getAttribute('data-jtml-renderer') || 'auto',
                controller: el.getAttribute('data-jtml-scene3d-controller') || '',
                state: clientState
              };
            }
          });
          const host = window.jtml3d && typeof window.jtml3d.render === 'function'
            ? window.jtml3d
            : null;
          if (host) {
            try {
              const hostResult = host.render(canvas, spec);
              canvas.dataset.jtmlScene3dStatus = 'host-rendered';
              publish(hostResult, 'host-rendered', true);
            } catch (err) {
              canvas.dataset.jtmlScene3dStatus = 'fallback';
              drawScene3DFallback(canvas, spec);
              publish({ error: err && err.message ? err.message : '3D renderer failed' }, 'fallback', false);
              reportStatus('error', err && err.message ? err.message : '3D renderer failed');
            }
          } else {
            canvas.dataset.jtmlScene3dStatus = 'fallback';
            drawScene3DFallback(canvas, spec);
            publish({}, 'fallback', false);
          }
          document.dispatchEvent(new CustomEvent('jtml:scene3d-ready', {
            detail: { canvas: canvas, spec: spec, controller: controllerName, hostRendered: !!host }
          }));
        });
      }

      function applyInitial() {
        bootstrapClientManifest();
        if (window.__jtml_bindings) applyBindings(window.__jtml_bindings);
        scanComponentInstances();
        renderDirectComponents();
        startDirectComponentBindings();
        startFetchBindings();
        startInvalidationBindings();
        startExternBindings();
        startRedirectBindings();
        startGuardBindings();
        collectRouteBindings();
        startLinkBindings();
        startDropzoneBindings();
        startMediaControllerBindings();
        startScene3DBindings();
        initTimelines();
        processImageBindings();
        applyClientState();
        applyRoutes();
      }
      if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', applyInitial);
      } else {
        applyInitial();
      }

      let ws = null;
      if (browserLocalRuntime) {
        reportStatus('browser-local', 'Browser-local runtime active');
      } else {
        try {
          const host = location.hostname || 'localhost';
          ws = new WebSocket('ws://' + host + ':' + wsPort);
          ws.onmessage = function (event) {
            const m = JSON.parse(event.data);
            reportStatus('connected', 'Runtime connected');
            if (m.type === 'populateBindings' && m.bindings) applyBindings(m.bindings);
            else if (m.type === 'updateBinding') {
              const el = document.getElementById(m.elementId);
              if (el) el.textContent = m.value;
            } else if (m.type === 'updateAttribute') {
              const el = document.getElementById(m.elementId);
              if (el) applyAttribute(el, m.attribute, m.value);
            } else if (m.type === 'reload') {
              // Structural change (source file edited in watch mode).
              // A full reload is safer than patching bindings, since the
              // element tree itself may have changed.
              location.reload();
            }
          };
          ws.onopen = function () { reportStatus('connected', 'Runtime connected'); };
          ws.onclose = function () { reportStatus('offline', 'Runtime disconnected; HTTP fallback available'); };
          ws.onerror = function () { reportStatus('fallback', 'WebSocket unavailable; using HTTP fallback'); };
        } catch (_) {
          reportStatus('fallback', 'WebSocket unavailable; using HTTP fallback');
        }
      }

      window.sendEvent = async function (elementId, eventType, args) {
        args = args || [];
        // Client-side intercept for refresh actions and redirect.
        const action = parseActionCall(args[0] || '');
        const fnName = action.name;
        if (fnName && __jtml_refresh_fns[fnName]) {
          await __jtml_refresh_fns[fnName]();
          return;
        }
        if (fnName && __jtml_extern_fns[fnName]) {
          await __jtml_extern_fns[fnName](action.args.concat(args.slice(1)));
          return;
        }
        if (fnName && __jtml_media_actions[fnName]) {
          await __jtml_media_actions[fnName](action.args.concat(args.slice(1)));
          return;
        }
        if (browserLocalRuntime && fnName && executeClientAction(fnName, action.args.concat(args.slice(1)))) {
          await runInvalidations(fnName);
          return;
        }
        if (browserLocalRuntime) {
          reportStatus('error', 'No browser-local action named: ' + (fnName || '(unknown)'));
          return;
        }
        try {
          const res = await fetch('/api/event', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ elementId: elementId, eventType: eventType, args: args })
          });
          const data = await res.json();
          if (data && data.bindings) applyBindings(data.bindings);
          if (data && data.error) reportStatus('error', data.error);
          if (fnName) await runInvalidations(fnName);
        } catch (err) {
          reportStatus('error', err && err.message ? err.message : 'event dispatch failed');
          console.error('[jtml] event dispatch failed:', err);
        }
      };

      function normalizeRoute(path) {
        if (!path || path === '#') return '/';
        if (path[0] === '#') path = path.slice(1);
        if (!path) return '/';
        return path[0] === '/' ? path : '/' + path;
      }

      function matchRouteParams(pattern, path) {
        if (String(pattern || '').trim() === '*') return {};
        pattern = normalizeRoute(pattern);
        path = normalizeRoute(path);
        const pp = pattern.split('/').filter(Boolean);
        const sp = path.split('/').filter(Boolean);
        if (pp.length !== sp.length) return null;
        const params = {};
        for (let i = 0; i < pp.length; i += 1) {
          if (pp[i][0] === ':') {
            params[pp[i].slice(1)] = decodeURIComponent(sp[i]);
            continue;
          }
          if (pp[i] !== sp[i]) return null;
        }
        return params;
      }

      function applyActiveLinkClasses(path) {
        document.querySelectorAll('[data-jtml-active-class]').forEach(function (el) {
          const cls = el.getAttribute('data-jtml-active-class');
          const href = el.getAttribute('data-jtml-href') || el.getAttribute('href') || '';
          const linkPath = normalizeRoute(href[0] === '#' ? href.slice(1) : href);
          if (cls) {
            if (linkPath === path) el.classList.add(cls);
            else el.classList.remove(cls);
          }
        });
      }

      const __jtml_guards = [];

      function startGuardBindings() {
        document.querySelectorAll('[data-jtml-route-guard]').forEach(function (meta) {
          const routePath = normalizeRoute(meta.getAttribute('data-jtml-route-guard') || '');
          const guardVar = meta.getAttribute('data-jtml-guard-var') || '';
          const redirectTo = meta.getAttribute('data-jtml-guard-redirect') || '';
          if (routePath && guardVar) {
            __jtml_guards.push({ path: routePath, var: guardVar, redirect: redirectTo });
          }
        });
      }

      function checkGuards(path) {
        for (var i = 0; i < __jtml_guards.length; i++) {
          var g = __jtml_guards[i];
          if (matchRouteParams(g.path, path) !== null && !clientState[g.var]) {
            if (g.redirect) window.__jtml_redirect(g.redirect);
            return false;
          }
        }
        return true;
      }

      function applyRoutes() {
        const routes = __jtml_routes.length ? __jtml_routes : collectRouteBindings();
        if (!routes.length) return;
        const path = normalizeRoute(location.hash || '/');
        clientState['activeRoute'] = path;
        applyActiveLinkClasses(path);
        if (!checkGuards(path)) return;
        let matched = false;
        let nextCurrentRoute = null;
        routes.forEach(function (record) {
          const route = record.element;
          const params = !matched ? matchRouteParams(record.path, path) : null;
          const isMatch = !!params;
          route.hidden = !isMatch;
          route.setAttribute('aria-hidden', isMatch ? 'false' : 'true');
          if (isMatch) {
            record.params.forEach(function (name) {
              clientState[name] = Object.prototype.hasOwnProperty.call(params, name) ? params[name] : '';
            });
            clientState['activeRouteName'] = record.name;
            nextCurrentRoute = {
              path: path,
              pattern: record.path,
              name: record.name,
              params: Object.assign({}, params),
              load: record.load.slice()
            };
            matched = true;
            runRouteLoads(route);
            applyClientState();
          }
        });
        if (!matched) {
          routes.forEach(function (record, index) {
            record.element.hidden = index !== 0;
            record.element.setAttribute('aria-hidden', index === 0 ? 'false' : 'true');
          });
          clientState['activeRouteName'] = '';
          nextCurrentRoute = { path: path, pattern: '', name: '', params: {}, load: [] };
        }
        __jtml_current_route = nextCurrentRoute;
        publishRouteApi();
        document.dispatchEvent(new CustomEvent('jtml:route-change', {
          detail: { route: __jtml_current_route }
        }));
      }

      window.addEventListener('hashchange', applyRoutes);
    })();
  </script>
)";
    return out.str();
}


} // namespace jtml
