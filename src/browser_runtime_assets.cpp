#include "jtml/browser_runtime_assets.h"

#include <array>

namespace jtml {
namespace {

const char* browserRuntimePreludeChunk() {
    return R"JTR0(
  <script>
    (function () {
      const wsPort = @JTML_WS_PORT@;
      const browserLocalRuntime = @JTML_BROWSER_LOCAL_RUNTIME@;
      const dynamicGeneratedUpdateFunctions = @JTML_DYNAMIC_GENERATED_UPDATE_FUNCTIONS@;

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
      const dynamicComponentInstances = {};
      const componentRenderPath = [];
      const dynamicComponentRenderStack = [];
      const componentBodyPlanFallbacks = [];
      const componentUpdatePlanCache = {};
      if (window.addEventListener) {
        window.addEventListener('jtml:static-update-plans-ready', function () {
          Object.keys(componentUpdatePlanCache).forEach(function (key) {
            delete componentUpdatePlanCache[key];
          });
          window.jtml = Object.assign(window.jtml || {}, {
            runtimeSecurity: Object.assign(
              {},
              window.jtml && window.jtml.runtimeSecurity || {},
              {
                staticUpdatePlansLoaded: true,
                staticUpdateFunctionsLoaded: !!window.__jtml_static_update_functions,
                staticComponentModulesLoaded: !!window.__jtml_static_component_modules
              }
            )
          });
        });
        window.addEventListener('jtml:static-component-modules-ready', function () {
          Object.keys(componentUpdatePlanCache).forEach(function (key) {
            delete componentUpdatePlanCache[key];
          });
          window.jtml = Object.assign(window.jtml || {}, {
            runtimeSecurity: Object.assign(
              {},
              window.jtml && window.jtml.runtimeSecurity || {},
              {
                staticComponentPlanIndexLoaded: true,
                staticUpdateFunctionsLoaded: !!window.__jtml_static_update_functions,
                staticComponentModulesLoaded: !!window.__jtml_static_component_modules
              }
            )
          });
        });
      }

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

      function parseComponentIntMap(value) {
        const map = {};
        String(value || '').split(';').forEach(function (entry) {
          if (!entry) return;
          const pos = entry.indexOf('=');
          if (pos === -1) return;
          const parsed = Number(entry.slice(pos + 1));
          map[entry.slice(0, pos)] = Number.isFinite(parsed) ? parsed : 0;
        });
        return map;
      }

      function parseComponentStringListMap(value) {
        const map = {};
        String(value || '').split(';').forEach(function (entry) {
          if (!entry) return;
          const pos = entry.indexOf('=');
          if (pos === -1) return;
          map[entry.slice(0, pos)] = entry.slice(pos + 1).split(',').filter(Boolean);
        });
        return map;
      }

      function escapeSelectorValue(value) {
        if (window.CSS && CSS.escape) return CSS.escape(value);
        return String(value || '').replace(/["\\]/g, '\\$&');
      }

      function componentElementFor(id) {
        if (!id) return null;
        return document.querySelector('[data-jtml-instance="' + escapeSelectorValue(id) + '"]') ||
          document.querySelector('[data-jtml-direct-instance="' + escapeSelectorValue(id) + '"]');
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

      function escapeAttribute(value) {
        return escapeHtml(value).replace(/'/g, '&#39;');
      }

      function componentTagName(name) {
        const map = {
          app: 'div',
          page: 'main',
          shell: 'div',
          topbar: 'header',
          sidebar: 'aside',
          content: 'main',
          panel: 'section',
          card: 'section',
          metric: 'article',
          toolbar: 'div',
          tabs: 'div',
          tab: 'button',
          alert: 'div',
          badge: 'span',
          modal: 'section',
          drawer: 'aside',
          toast: 'div',
          loading: 'div',
          error: 'div',
          empty: 'div',
          field: 'label',
          spacer: 'div',
          box: 'div',
          stack: 'div',
          cluster: 'div',
          split: 'div',
          grid: 'div',
          text: 'p',
          link: 'a',
          navlink: 'a',
          image: 'img',
          list: 'ul',
          listOrdered: 'ol',
          item: 'li',
          checkbox: 'input',
          file: 'input',
          dropzone: 'input',
          title: 'h1',
          subtitle: 'p'
        };
        return map[name] || name;
      }

      function semanticUiPrimitiveNames() {
        return {
          app: true,
          shell: true,
          topbar: true,
          sidebar: true,
          content: true,
          panel: true,
          card: true,
          metric: true,
          stack: true,
          cluster: true,
          split: true,
          grid: true,
          toolbar: true,
          tabs: true,
          tab: true,
          alert: true,
          badge: true,
          modal: true,
          drawer: true,
          toast: true,
          loading: true,
          error: true,
          empty: true,
          field: true,
          spacer: true
        };
      }

      function isSemanticUiPrimitiveName(name) {
        return !!semanticUiPrimitiveNames()[String(name || '')];
      }

      function semanticUiModifierNames() {
        return {
          cols: true,
          gap: true,
          pad: true,
          radius: true,
          shadow: true,
          tone: true,
          align: true,
          justify: true,
          width: true,
          surface: true
        };
      }

      function isSemanticUiModifierName(name) {
        return !!semanticUiModifierNames()[String(name || '')];
      }

      function componentAttributeNames() {
        return {
          id: true,
          class: true,
          style: true,
          role: true,
          href: true,
          src: true,
          alt: true,
          title: true,
          type: true,
          name: true,
          value: true,
          placeholder: true,
          for: true,
          rel: true,
          target: true,
          method: true,
          action: true,
          enctype: true,
          autocomplete: true,
          inputmode: true,
          pattern: true,
          accept: true,
          poster: true,
          preload: true,
          kind: true,
          srclang: true,
          label: true,
          width: true,
          height: true,
          min: true,
          max: true,
          step: true,
          minlength: true,
          maxlength: true,
          rows: true,
          cols: true,
          cx: true,
          cy: true,
          r: true,
          x: true,
          y: true,
          x1: true,
          y1: true,
          x2: true,
          y2: true,
          d: true,
          points: true,
          viewBox: true,
          fill: true,
          stroke: true,
          'stroke-width': true,
          transform: true,
          opacity: true,
          to: true,
          'active-class': true
        };
      }

      function isComponentAttributeName(name) {
        name = String(name || '');
        return !!componentAttributeNames()[name] ||
          name.indexOf('aria-') === 0 ||
          name.indexOf('data-') === 0;
      }

      function isComponentBooleanAttribute(name) {
        return {
          disabled: true,
          required: true,
          checked: true,
          selected: true,
          multiple: true,
          readonly: true,
          autofocus: true,
          playsinline: true,
          open: true,
          hidden: true,
          controls: true,
          autoplay: true,
          loop: true,
          muted: true
        }[String(name || '')] === true;
      }

      function componentAttributeTakesValue(name) {
        return isComponentAttributeName(name) && !isComponentBooleanAttribute(name);
      }

      function sameRuntimeModule(left, right) {
        if (left == null || right == null) return false;
        return String(left) === String(right);
      }

      function recordComponentBodyPlanFallback(instance, definition, node, reason, actionContext) {
        const diagnostic = {
          componentId: instance && instance.id || '',
          component: instance && instance.component || definition && definition.name || '',
          moduleId: definition && definition.moduleId != null ? definition.moduleId : null,
          componentSourceLine: definition && definition.sourceLine || 0,
          action: actionContext && actionContext.name || '',
          actionSourceLine: actionContext && actionContext.sourceLine || 0,
          actionText: actionContext && actionContext.text || '',
          bodySourceLine: node && node.sourceLine || 0,
          nodeText: node && node.text || '',
          nodeKind: node && node.kind || '',
          reason: reason || 'unsupported component body-plan node'
        };
        componentBodyPlanFallbacks.push(diagnostic);
        window.jtml = Object.assign(window.jtml || {}, {
          directComponentFallbacks: componentBodyPlanFallbacks
        });
        if (window.console && console.warn) {
          console.warn('[jtml] direct component fallback:', diagnostic);
        }
        return false;
      }

      function componentDefinitionForInstance(instance) {
        if (!instance) return null;
        const name = instance.component || '';
        if (instance.definitionModule != null) {
          const byDefinitionModule = componentDefinitions.find(function (item) {
            return item.name === name && sameRuntimeModule(item.moduleId, instance.definitionModule);
          });
          if (byDefinitionModule) return byDefinitionModule;
        }
        if (instance.moduleId != null) {
          const byInstanceModule = componentDefinitions.find(function (item) {
            return item.name === name && sameRuntimeModule(item.moduleId, instance.moduleId);
          });
          if (byInstanceModule) return byInstanceModule;
        }
        return componentDefinitions.find(function (item) { return item.name === name; }) || null;
      }

      function componentDefinitionForName(name, moduleId, fallbackModuleId) {
        if (moduleId != null) {
          const byModule = componentDefinitions.find(function (item) {
            return item.name === name && sameRuntimeModule(item.moduleId, moduleId);
          });
          if (byModule) return byModule;
        }
        if (fallbackModuleId != null) {
          const byFallback = componentDefinitions.find(function (item) {
            return item.name === name && sameRuntimeModule(item.moduleId, fallbackModuleId);
          });
          if (byFallback) return byFallback;
        }
        return componentDefinitions.find(function (item) { return item.name === name; }) || null;
      }

      function findRuntimeComponentInstance(id) {
        if (!id) return null;
        const staticInstance = componentInstances.find(function (item) { return item.id === id; });
        if (staticInstance) return staticInstance;
        return dynamicComponentInstances[id] || null;
      }

      function registerDynamicComponentInstance(instance) {
        if (!instance || !instance.id) return instance;
        if (dynamicComponentRenderStack.length) {
          dynamicComponentRenderStack[dynamicComponentRenderStack.length - 1].add(instance.id);
        }
        const existing = dynamicComponentInstances[instance.id];
        if (existing) {
          existing.moduleId = instance.moduleId;
          existing.definitionModule = instance.definitionModule;
          existing.component = instance.component;
          existing.params = instance.params || existing.params || {};
          existing.locals = instance.locals || existing.locals || {};
          existing.eventHandlers = instance.eventHandlers || existing.eventHandlers || {};
          existing.slotHtml = instance.slotHtml != null ? instance.slotHtml : existing.slotHtml;
          existing.slotPlan = Array.isArray(instance.slotPlan) ? instance.slotPlan.slice() : (existing.slotPlan || []);
          existing.element = componentElementFor(instance.id) || existing.element || null;
          return existing;
        }
        instance.element = componentElementFor(instance.id) || null;
        dynamicComponentInstances[instance.id] = instance;
        return instance;
      }

      function pruneDynamicComponentSubtree(parentId, activeIds) {
        if (!parentId) return;
        const prefix = String(parentId) + '__';
        Object.keys(dynamicComponentInstances).forEach(function (id) {
          if (id.indexOf(prefix) !== 0) return;
          if (activeIds && activeIds.has(id)) return;
          delete dynamicComponentInstances[id];
          delete componentScopes[id];
        });
      }

      function pruneDynamicComponentListItem(parentId, nodeIndex, key) {
        if (!parentId) return 0;
        const prefix = String(parentId) + '__';
        const marker = '_for' + String(nodeIndex < 0 ? 'x' : nodeIndex) + '_' + componentPathSegment(key);
        let removed = 0;
        Object.keys(dynamicComponentInstances).forEach(function (id) {
          if (id.indexOf(prefix) !== 0 || id.indexOf(marker) === -1) return;
          delete dynamicComponentInstances[id];
          delete componentScopes[id];
          removed += 1;
        });
        return removed;
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

      function addComponentNameSet(target, values) {
        (values || []).forEach(function (value) {
          if (value != null && String(value)) target[String(value)] = true;
        });
      }

      function componentRenderedReadSet(definition) {
        const reads = {};
        const derivedDeps = {};
        (definition && definition.bodyPlan || []).forEach(function (node) {
          if (!node) return;
          if (node.kind === 'derived' && node.name) {
            derivedDeps[node.name] = (node.reads || []).slice();
          }
          if (node.kind === 'template' || node.kind === 'slot') {
            addComponentNameSet(reads, node.reads || []);
          }
        });
        let changed = true;
        let guard = 0;
        while (changed && guard++ < 1000) {
          changed = false;
          Object.keys(derivedDeps).forEach(function (name) {
            if (!reads[name]) return;
            (derivedDeps[name] || []).forEach(function (dep) {
              dep = String(dep || '');
              if (!dep || reads[dep]) return;
              reads[dep] = true;
              changed = true;
            });
          });
        }
        return reads;
      }

      function componentWritesAffectRender(definition, writes) {
        const changed = {};
        addComponentNameSet(changed, writes || []);
        const names = Object.keys(changed);
        if (!names.length) return true;
        const renderedReads = componentRenderedReadSet(definition);
        return names.some(function (name) { return !!renderedReads[name]; });
      }

      function componentNodeIndex(definition, node) {
        return (definition && definition.bodyPlan || []).indexOf(node);
      }

      function componentNodeReadSet(definition, node) {
        const reads = {};
        addComponentNameSet(reads, node && node.reads || []);
        const derivedDeps = {};
        (definition && definition.bodyPlan || []).forEach(function (candidate) {
          if (candidate && candidate.kind === 'derived' && candidate.name) {
            derivedDeps[candidate.name] = (candidate.reads || []).slice();
          }
        });
        let changed = true;
        let guard = 0;
        while (changed && guard++ < 1000) {
          changed = false;
          Object.keys(derivedDeps).forEach(function (name) {
            if (!reads[name]) return;
            (derivedDeps[name] || []).forEach(function (dep) {
              dep = String(dep || '');
              if (!dep || reads[dep]) return;
              reads[dep] = true;
              changed = true;
            });
          });
        }
        return reads;
      }

      function componentChangeAffectsNode(definition, node, writes) {
        const changed = {};
        addComponentNameSet(changed, writes || []);
        const names = Object.keys(changed);
        if (!names.length) return true;
        const reads = componentNodeReadSet(definition, node);
        return names.some(function (name) { return !!reads[name]; });
      }

      function noteComponentActionResult(instance, definition, actionName, writes, renderSkipped) {
        const renderedReads = componentRenderedReadSet(definition);
        window.jtml = Object.assign(window.jtml || {}, {
          directComponentLastAction: {
            componentId: instance && instance.id || '',
            component: instance && instance.component || definition && definition.name || '',
            action: actionName || '',
            writes: Object.keys(writes || {}),
            renderRelevantReads: Object.keys(renderedReads),
            renderSkipped: !!renderSkipped
          }
        });
      }

      function componentNodeChildren(definition, node) {
        return (node.childIndices || [])
          .map(function (index) { return (definition.bodyPlan || [])[index]; })
          .filter(Boolean);
      }

      function componentNodeHead(node) {
        const words = splitComponentWords(node && node.text ? node.text : '');
        return node && (node.name || words[0]) || '';
      }

      function isElseComponentNode(node) {
        return componentNodeHead(node) === 'else';
      }

      function componentElseSibling(definition, node) {
        const plan = definition.bodyPlan || [];
        const index = plan.indexOf(node);
        if (index === -1) return null;
        for (let i = index + 1; i < plan.length; i += 1) {
          const candidate = plan[i];
          if (!candidate) continue;
          if (candidate.parentIndex === node.parentIndex) {
            return isElseComponentNode(candidate) ? candidate : null;
          }
        }
        return null;
      }

      function renderComponentChildren(definition, instance, node, scope) {
        return componentNodeChildren(definition, node)
          .filter(function (child) { return !isElseComponentNode(child); })
          .map(function (child) { return renderComponentPlanNode(definition, instance, child, scope); })
          .join('');
      }

      function componentSlotName(node) {
        const words = splitComponentWords(node && node.text ? node.text : '');
        return words[0] === 'slot' && words.length > 1 ? words[1] : '';
      }

      function renderComponentSlot(instance, scope, name) {
        const requestedName = String(name || '');
        if (!requestedName && instance && instance.slotHtml != null) return String(instance.slotHtml);
        if (requestedName && instance && instance.slotHtmlByName &&
            Object.prototype.hasOwnProperty.call(instance.slotHtmlByName, requestedName)) {
          return String(instance.slotHtmlByName[requestedName]);
        }
        const slotPlan = instance && Array.isArray(instance.slotPlan) ? instance.slotPlan : [];
        if (!slotPlan.length) return '';
        const slotDefinition = { name: '__slot', bodyPlan: slotPlan };
        const output = [];
        slotPlan.forEach(function (node) {
          if (!node || node.parentIndex !== -1 ||
              (node.kind !== 'template' && node.kind !== 'slot')) {
            return;
          }
          if (componentNodeHead(node) === 'slot') {
            const slotName = componentSlotName(node);
            if (slotName === requestedName) {
              output.push(renderComponentChildren(slotDefinition, instance, node, scope));
            }
            return;
          }
          if (!requestedName) {
            output.push(renderComponentPlanNode(slotDefinition, instance, node, scope));
          }
        });
        return output.join('');
      }

      function slotPlanForComponentNode(definition, node) {
        const plan = definition && Array.isArray(definition.bodyPlan) ? definition.bodyPlan : [];
        const output = [];
        function cloneSubtree(source, parentIndex) {
          if (!source) return -1;
          const clone = Object.assign({}, source);
          const index = output.length;
          clone.parentIndex = parentIndex;
          clone.childIndices = [];
          output.push(clone);
          (source.childIndices || []).forEach(function (childIndex) {
            const child = plan[childIndex];
            const clonedChildIndex = cloneSubtree(child, index);
            if (clonedChildIndex >= 0) clone.childIndices.push(clonedChildIndex);
          });
          return index;
        }
        (node && node.childIndices || []).forEach(function (childIndex) {
          cloneSubtree(plan[childIndex], -1);
        });
        return output;
      }

      function renderComponentExpression(expr, scope) {
        const literal = unquoteComponentText(expr);
        if (literal !== String(expr || '').trim()) return literal;
        const result = evaluateClientExpression(expr, scope);
        return result.found ? renderTemplateValue(result.value) : literal;
      }

      function evaluateComponentValue(expr, scope) {
        const literal = unquoteComponentText(expr);
        if (literal !== String(expr || '').trim()) return literal;
        const result = evaluateClientExpression(expr, scope);
        return result.found ? result.value : literal;
      }

      function componentExpressionBalanced(expr, begin, end) {
        let paren = 0;
        let bracket = 0;
        let brace = 0;
        let quote = '';
        for (let i = begin; i < end; i += 1) {
          const ch = expr[i];
          if (quote) {
            if (ch === '\\' && i + 1 < end) i += 1;
            else if (ch === quote) quote = '';
            continue;
          }
          if (ch === '"' || ch === "'") {
            quote = ch;
            continue;
          }
          if (ch === '(') paren += 1;
          else if (ch === ')') paren -= 1;
          else if (ch === '[') bracket += 1;
          else if (ch === ']') bracket -= 1;
          else if (ch === '{') brace += 1;
          else if (ch === '}') brace -= 1;
          if (paren < 0 || bracket < 0 || brace < 0) return false;
        }
        return !quote && paren === 0 && bracket === 0 && brace === 0;
      }

      function stripOuterComponentExpressionParens(expr) {
        expr = String(expr || '').trim();
        while (expr.length >= 2 && expr[0] === '(' && expr[expr.length - 1] === ')' &&
               componentExpressionBalanced(expr, 1, expr.length - 1)) {
          expr = expr.slice(1, -1).trim();
        }
        return expr;
      }

      function findTopLevelComponentChar(expr, needle) {
        let paren = 0;
        let bracket = 0;
        let brace = 0;
        let quote = '';
        for (let i = 0; i < expr.length; i += 1) {
          const ch = expr[i];
          if (quote) {
            if (ch === '\\' && i + 1 < expr.length) i += 1;
            else if (ch === quote) quote = '';
            continue;
          }
          if (ch === '"' || ch === "'") {
            quote = ch;
            continue;
          }
          if (ch === '(') paren += 1;
          else if (ch === ')') paren -= 1;
          else if (ch === '[') bracket += 1;
          else if (ch === ']') bracket -= 1;
          else if (ch === '{') brace += 1;
          else if (ch === '}') brace -= 1;
          else if (ch === needle && paren === 0 && bracket === 0 && brace === 0) return i;
        }
        return -1;
      }

      function findMatchingTopLevelComponentColon(expr, question) {
        let paren = 0;
        let bracket = 0;
        let brace = 0;
        let nested = 0;
        let quote = '';
        for (let i = question + 1; i < expr.length; i += 1) {
          const ch = expr[i];
          if (quote) {
            if (ch === '\\' && i + 1 < expr.length) i += 1;
            else if (ch === quote) quote = '';
            continue;
          }
          if (ch === '"' || ch === "'") {
            quote = ch;
            continue;
          }
          if (ch === '(') paren += 1;
          else if (ch === ')') paren -= 1;
          else if (ch === '[') bracket += 1;
          else if (ch === ']') bracket -= 1;
          else if (ch === '{') brace += 1;
          else if (ch === '}') brace -= 1;
          else if (paren === 0 && bracket === 0 && brace === 0) {
            if (ch === '?') nested += 1;
            else if (ch === ':') {
              if (nested === 0) return i;
              nested -= 1;
            }
          }
        }
        return -1;
      }

      function componentOperatorMayBeUnary(expr, pos) {
        let cursor = pos;
        while (cursor > 0 && /\s/.test(expr[cursor - 1])) cursor -= 1;
        if (cursor === 0) return true;
        return /[\(\[\{\?:,\+\-\*\/%<>=!&|]/.test(expr[cursor - 1]);
      }

      function findTopLevelComponentOperator(expr, operators) {
        let paren = 0;
        let bracket = 0;
        let brace = 0;
        let quote = '';
        for (let i = expr.length - 1; i >= 0; i -= 1) {
          const ch = expr[i];
          if (quote) {
            if (ch === quote) quote = '';
            continue;
          }
          if (ch === '"' || ch === "'") {
            quote = ch;
            continue;
          }
          if (ch === ')') paren += 1;
          else if (ch === '(') paren -= 1;
          else if (ch === ']') bracket += 1;
          else if (ch === '[') bracket -= 1;
          else if (ch === '}') brace += 1;
          else if (ch === '{') brace -= 1;
          if (paren !== 0 || bracket !== 0 || brace !== 0) continue;
          for (let j = 0; j < operators.length; j += 1) {
            const op = operators[j];
            if (expr.slice(i, i + op.length) !== op) continue;
            if ((op === '+' || op === '-') && componentOperatorMayBeUnary(expr, i)) continue;
            return { index: i, operator: op };
          }
        }
        return null;
      }

      function compileComponentExpressionPlan(expr) {
        expr = stripOuterComponentExpressionParens(expr);
        if (!expr) return { kind: 'empty', source: '' };
        const literal = unquoteComponentText(expr);
        if (literal !== expr) return { kind: 'literal', source: expr, value: literal };
        if (expr === 'true' || expr === 'false') {
          return { kind: 'literal', source: expr, value: expr === 'true' };
        }
        if (expr === 'null') return { kind: 'literal', source: expr, value: null };
        if (/^-?(?:\d+|\d*\.\d+)$/.test(expr)) {
          return { kind: 'literal', source: expr, value: Number(expr) };
        }
        if (/^[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*$/.test(expr)) {
          const segments = expr.split('.');
          return { kind: 'path', source: expr, root: segments[0], segments: segments };
        }
        const question = findTopLevelComponentChar(expr, '?');
        if (question > 0) {
          const colon = findMatchingTopLevelComponentColon(expr, question);
          if (colon > question) {
            const test = expr.slice(0, question).trim();
            const whenTrue = expr.slice(question + 1, colon).trim();
            const whenFalse = expr.slice(colon + 1).trim();
            if (test && whenTrue && whenFalse) {
              return {
                kind: 'conditional',
                source: expr,
                test: compileComponentExpressionPlan(test),
                whenTrue: compileComponentExpressionPlan(whenTrue),
                whenFalse: compileComponentExpressionPlan(whenFalse)
              };
            }
          }
        }
        const groups = [
          ['||'],
          ['&&'],
          ['==', '!='],
          ['<=', '>=', '<', '>'],
          ['+', '-'],
          ['*', '/', '%']
        ];
        for (let i = 0; i < groups.length; i += 1) {
          const found = findTopLevelComponentOperator(expr, groups[i]);
          if (!found || found.index <= 0) continue;
          const left = expr.slice(0, found.index).trim();
          const right = expr.slice(found.index + found.operator.length).trim();
          if (!left || !right) continue;
          return {
            kind: 'binary',
            source: expr,
            operator: found.operator,
            left: compileComponentExpressionPlan(left),
            right: compileComponentExpressionPlan(right)
          };
        }
        if (expr.length > 1 && expr[0] === '!') {
          const argument = expr.slice(1).trim();
          if (argument) {
            return {
              kind: 'unary',
              source: expr,
              operator: '!',
              argument: compileComponentExpressionPlan(argument)
            };
          }
        }
        return { kind: 'source', source: expr };
      }

      function evaluateCompiledComponentExpression(plan, fallbackExpr, scope) {
        if (typeof plan === 'function') return plan(scope || {});
        const result = evaluateCompiledComponentExpressionResult(plan, fallbackExpr, scope);
        if (result.found) return result.value;
        return evaluateComponentValue(fallbackExpr || '', scope);
      }

      function evaluateCompiledComponentExpressionResult(plan, fallbackExpr, scope) {
        if (typeof plan === 'function') return { found: true, value: plan(scope || {}) };
        if (!plan || typeof plan !== 'object') return evaluateClientExpression(fallbackExpr || '', scope);
        if (plan.kind === 'empty') return { found: true, value: '' };
        if (plan.kind === 'literal') return { found: true, value: plan.value };
        if (plan.kind === 'path') {
          const segments = Array.isArray(plan.segments) && plan.segments.length
            ? plan.segments
            : String(plan.source || fallbackExpr || '').split('.').filter(Boolean);
          if (!segments.length) return { found: true, value: '' };
          if (!scope || !Object.prototype.hasOwnProperty.call(scope, segments[0])) {
            return { found: false, value: undefined };
          }
          let current = scope[segments[0]];
          for (let i = 1; i < segments.length; i += 1) {
            if (current == null) return { found: true, value: '' };
            current = current[segments[i]];
          }
          return { found: true, value: current == null ? '' : current };
        }
        if (plan.kind === 'unary' && plan.operator === '!') {
          const argument = evaluateCompiledComponentExpressionResult(plan.argument, '', scope);
          return argument.found
            ? { found: true, value: !argument.value }
            : evaluateClientExpression(plan.source || fallbackExpr || '', scope);
        }
        if (plan.kind === 'binary') {
          const left = evaluateCompiledComponentExpressionResult(plan.left, '', scope);
          const right = evaluateCompiledComponentExpressionResult(plan.right, '', scope);
          if (!left.found || !right.found) {
            return evaluateClientExpression(plan.source || fallbackExpr || '', scope);
          }
          switch (plan.operator) {
            case '||': return { found: true, value: left.value || right.value };
            case '&&': return { found: true, value: left.value && right.value };
            case '==': return { found: true, value: left.value == right.value };
            case '!=': return { found: true, value: left.value != right.value };
            case '<=': return { found: true, value: left.value <= right.value };
            case '>=': return { found: true, value: left.value >= right.value };
            case '<': return { found: true, value: left.value < right.value };
            case '>': return { found: true, value: left.value > right.value };
            case '+': return { found: true, value: left.value + right.value };
            case '-': return { found: true, value: left.value - right.value };
            case '*': return { found: true, value: left.value * right.value };
            case '/': return { found: true, value: left.value / right.value };
            case '%': return { found: true, value: left.value % right.value };
          }
        }
        if (plan.kind === 'conditional') {
          const test = evaluateCompiledComponentExpressionResult(plan.test, '', scope);
          if (!test.found) return evaluateClientExpression(plan.source || fallbackExpr || '', scope);
          return evaluateCompiledComponentExpressionResult(
            test.value ? plan.whenTrue : plan.whenFalse,
            '',
            scope);
        }
        return evaluateClientExpression(plan.source || fallbackExpr || '', scope);
      }

      function renderCompiledComponentExpression(plan, fallbackExpr, scope) {
        return renderTemplateValue(evaluateCompiledComponentExpression(plan, fallbackExpr, scope));
      }

      function parseComponentElementParts(words, start, scope, stopWords) {
        return evaluateCompiledComponentElementParts(
          compileComponentElementParts(words, start, stopWords),
          scope);
      }

      function compileComponentElementParts(words, start, stopWords) {
        const attrs = [];
        const modifiers = [];
        const content = [];
        const stops = stopWords || {};
        for (let i = start; i < words.length; i += 1) {
          const token = words[i];
          if (stops[token]) break;
          if (isSemanticUiModifierName(token)) {
            const raw = i + 1 < words.length ? words[i + 1] : '';
            if (raw && !stops[raw] && !isComponentAttributeName(raw) &&
                !isComponentBooleanAttribute(raw) && !isSemanticUiModifierName(raw)) {
              modifiers.push({ name: token, raw: raw, exprPlan: compileComponentExpressionPlan(raw) });
              i += 1;
            } else {
              modifiers.push({ name: token, raw: '' });
            }
            continue;
          }
          if (isComponentAttributeName(token)) {
            const raw = i + 1 < words.length ? words[i + 1] : '';
            if (raw && !stops[raw] &&
                (componentAttributeTakesValue(token) ||
                 (!isComponentAttributeName(raw) && !isComponentBooleanAttribute(raw)))) {
              attrs.push({ name: token, raw: raw, exprPlan: compileComponentExpressionPlan(raw) });
              i += 1;
            } else {
              content.push(token);
            }
            continue;
          }
          if (isComponentBooleanAttribute(token)) {
            attrs.push({ name: token, boolean: true });
            continue;
          }
          content.push(token);
        }
        const contentSource = content.join(' ');
        return {
          content: contentSource,
          contentPlan: compileComponentExpressionPlan(contentSource),
          attrs: attrs,
          modifiers: modifiers
        };
      }

      function evaluateCompiledComponentElementParts(plan, scope) {
        return {
          content: plan && plan.content || '',
          contentPlan: plan && plan.contentPlan || compileComponentExpressionPlan(plan && plan.content || ''),
          attrs: (plan && plan.attrs || []).map(function (attr) {
            if (!attr || !attr.name) return attr;
            if (attr.boolean) return { name: attr.name, boolean: true };
            return {
              name: attr.name,
              value: renderCompiledComponentExpression(attr.exprPlan, attr.raw || '', scope)
            };
          }).filter(Boolean),
          modifiers: (plan && plan.modifiers || []).map(function (modifier) {
            if (!modifier || !modifier.name) return modifier;
            return {
              name: modifier.name,
              value: modifier.raw ? renderCompiledComponentExpression(modifier.exprPlan, modifier.raw, scope) : ''
            };
          }).filter(Boolean)
        };
      }

      function renderComponentAttributes(parts, extra) {
        const rendered = componentAttributeMap(parts, extra);
        const attrs = [];
        Object.keys(rendered).forEach(function (name) {
          const value = rendered[name];
          if (value === true) attrs.push(' ' + name);
          else attrs.push(' ' + name + '="' + escapeAttribute(value) + '"');
        });
        return attrs.join('');
      }

      function componentAttributeMap(parts, extra) {
        const rendered = {};
        let classValue = '';
        (parts && parts.attrs || []).forEach(function (attr) {
          if (!attr || !attr.name) return;
          if (attr.name === 'class' && !attr.boolean) {
            classValue = (classValue ? classValue + ' ' : '') + attr.value;
          } else if (attr.boolean) rendered[attr.name] = true;
          else rendered[attr.name] = attr.value;
        });
        Object.keys(extra || {}).forEach(function (name) {
          if (extra[name] == null || extra[name] === '') return;
          if (name === 'class') classValue = (classValue ? classValue + ' ' : '') + extra[name];
          else rendered[name] = extra[name];
        });
        if (classValue) rendered.class = classValue;
        return rendered;
      }

      function componentManagedAttributeMarker(parts, extra) {
        const names = Object.keys(componentAttributeMap(parts, extra));
        if (!names.length) return '';
        return ' data-jtml-direct-managed-attrs="' + escapeAttribute(names.join(',')) + '"';
      }

      function componentPartsHaveAttribute(parts, name) {
        return (parts && parts.attrs || []).some(function (attr) {
          return attr && attr.name === name;
        });
      }

      function semanticUiRenderExtras(head, parts) {
        const extras = {};
        const classes = [];
        if (isSemanticUiPrimitiveName(head)) {
          classes.push('jtml-' + head);
          extras['data-jtml-ui'] = head;
        }
        (parts && parts.modifiers || []).forEach(function (modifier) {
          if (!modifier || !modifier.name) return;
          const suffix = modifier.value ? '-' + String(modifier.value).replace(/[^A-Za-z0-9_-]/g, '-') : '';
          classes.push('jtml-' + modifier.name + suffix);
          extras['data-jtml-ui-' + modifier.name] = modifier.value || 'true';
        });
        if (classes.length) extras.class = classes.join(' ');
        return extras;
      }

      function componentPathSegment(value) {
        const raw = String(value == null ? '' : value);
        const safe = raw.replace(/[^A-Za-z0-9_-]/g, '_').replace(/^_+|_+$/g, '');
        return safe || 'empty';
      }

      function parseComponentActionInvocation(raw, scope) {
        return evaluateCompiledComponentActionInvocation(
          compileComponentActionInvocation(raw),
          scope);
      }

      function compileComponentActionInvocation(raw) {
        raw = String(raw || '').trim();
        const match = raw.match(/^([A-Za-z_][A-Za-z0-9_\.]*)(?:\((.*)\))?$/);
        if (!match) return { name: raw, argExpressions: [] };
        const argSource = String(match[2] || '').trim();
        const argExpressions = argSource ? splitTopLevelList(argSource) : [];
        return {
          name: match[1],
          argExpressions: argExpressions,
          argPlans: argExpressions.map(compileComponentExpressionPlan)
        };
      }

      function evaluateCompiledComponentActionInvocation(compiled, scope) {
        const argExpressions = compiled && compiled.argExpressions || [];
        const argPlans = compiled && compiled.argPlans || [];
        return {
          name: compiled && compiled.name || '',
          args: argExpressions.map(function (expr, index) {
            return evaluateCompiledComponentExpression(argPlans[index], expr, scope);
          })
        };
      }

      function parseComponentEventHandlers(words, paramCount, scope) {
        const handlers = {};
        let i = 1 + Number(paramCount || 0);
        while (i < words.length) {
          if (words[i] !== 'on') {
            i += 1;
            continue;
          }
          if (i + 2 >= words.length) break;
          const eventName = words[i + 1] || '';
          const handler = parseComponentActionInvocation(words[i + 2] || '', scope);
          if (eventName && handler && handler.name) handlers[eventName] = handler;
          i += 3;
        }
        return handlers;
      }

)JTR0";
}

const char* browserRuntimeComponentChunk() {
    return R"JTR1(      function renderNestedComponentCall(parentDefinition, parentInstance, node, scope, name, directNodeAttr) {
        const nestedDefinition = componentDefinitionForName(
          name,
          node && node.definitionModule != null
            ? node.definitionModule
            : parentDefinition && parentDefinition.moduleId,
          parentInstance && parentInstance.moduleId
        );
        if (!nestedDefinition) return null;
        const words = splitComponentWords(node.text || '');
        const params = {};
        (nestedDefinition.params || []).forEach(function (param, index) {
          const raw = words[index + 1] || '';
          const plan = node && Array.isArray(node.wordPlans) ? node.wordPlans[index + 1] : null;
          params[param] = evaluateCompiledComponentExpression(plan, raw, scope);
        });
        const index = (parentDefinition.bodyPlan || []).indexOf(node);
        const pathSuffix = componentRenderPath.length
          ? '_' + componentRenderPath.join('_')
          : '';
        const nestedInstance = {
          moduleId: parentDefinition && parentDefinition.moduleId,
          definitionModule: nestedDefinition.moduleId,
          id: String(parentInstance && parentInstance.id || 'component') + '__' +
            name + '_' + String(index < 0 ? 'x' : index) + pathSuffix,
          parentId: parentInstance && parentInstance.id || '',
          component: name,
          params: params,
          locals: {},
          eventHandlers: parseComponentEventHandlers(words, (nestedDefinition.params || []).length, scope),
          slotPlan: slotPlanForComponentNode(parentDefinition, node),
          slotHtml: renderComponentChildren(parentDefinition, parentInstance, node, scope)
        };
        registerDynamicComponentInstance(nestedInstance);
        const nestedScope = componentScopeFor(nestedInstance, nestedDefinition);
        applyComponentDerived(nestedScope, nestedDefinition);
        const html = (nestedDefinition.bodyPlan || [])
          .filter(function (candidate) {
            return candidate && candidate.renderRoot && candidate.kind === 'template';
          })
          .map(function (candidate) {
            return renderComponentPlanNode(nestedDefinition, nestedInstance, candidate, nestedScope);
          })
          .join('');
        return '<div' + (directNodeAttr || '') +
          ' data-jtml-direct-instance="' + escapeAttribute(nestedInstance.id) + '"' +
          ' data-jtml-component="' + escapeAttribute(name) + '"' +
          ' data-jtml-component-parent="' + escapeAttribute(nestedInstance.parentId || '') + '"' +
          ' data-jtml-nested-component="true">' + html + '</div>';
      }

      function renderComponentPlanNode(definition, instance, node, scope) {
        if (!node || (node.kind !== 'template' && node.kind !== 'slot')) return '';
        const words = splitComponentWords(node.text || '');
        const head = componentNodeHead(node) || 'box';
        if (head === 'else') return '';
        const directNodeIndex = componentNodeIndex(definition, node);
        const directNodeAttr = directNodeIndex >= 0
          ? ' data-jtml-direct-body-node="' + String(directNodeIndex) + '"'
          : '';
        if (head === 'slot') {
          return '<span' + directNodeAttr +
            ' data-jtml-direct-region="slot" style="display:contents">' +
            renderComponentSlot(instance, scope, words[1] || '') +
            '</span>';
        }
        if (head === 'if') {
          const conditionExpr = node.expression || words.slice(1).join(' ');
          const condition = evaluateCompiledComponentExpressionResult(
            node.expressionPlan || compileComponentExpressionPlan(conditionExpr),
            conditionExpr,
            scope);
          let html = '';
          if (condition.found && condition.value) {
            html = renderComponentChildren(definition, instance, node, scope);
          } else {
            const elseNode = componentElseSibling(definition, node);
            html = elseNode ? renderComponentChildren(definition, instance, elseNode, scope) : '';
          }
          return '<span' + directNodeAttr +
            ' data-jtml-direct-region="if" style="display:contents">' +
            html + '</span>';
        }
        if (head === 'for') {
          const html = renderComponentForRegion(definition, instance, node, scope, words);
          return '<span' + directNodeAttr +
            ' data-jtml-direct-region="for" style="display:contents">' +
            html + '</span>';
        }
        const children = renderComponentChildren(definition, instance, node, scope);
        const nested = renderNestedComponentCall(definition, instance, node, scope, head, directNodeAttr);
        if (nested != null) return nested;
        if (head === 'show' || head === 'text') {
          const expr = head === 'show'
            ? (node.expression || words.slice(1).join(' '))
            : (node.expression || words.slice(1).join(' '));
          return '<p' + directNodeAttr + ' data-jtml-direct-text-node="true">' +
            escapeHtml(renderCompiledComponentExpression(node.expressionPlan, expr, scope)) + '</p>' + children;
        }
        if (head === 'button') {
          const clickIndex = words.indexOf('click');
          const parts = parseComponentElementParts(
            words,
            1,
            scope,
            { click: true }
          );
          const labelExpr = parts.content;
          const invocation = clickIndex === -1
            ? { name: '', args: [] }
            : parseComponentActionInvocation(words[clickIndex + 1] || '', scope);
          const label = renderCompiledComponentExpression(parts.contentPlan, labelExpr || '"Button"', scope);
          const attrs = invocation.name
            ? ' data-jtml-direct-component-id="' + escapeHtml(instance.id) + '"' +
              ' data-jtml-direct-component-action="' + escapeHtml(invocation.name) + '"' +
              ' data-jtml-direct-component-args="' + escapeAttribute(JSON.stringify(invocation.args || [])) + '"'
            : '';
          const extras = semanticUiRenderExtras(head, parts);
          return '<button type="button"' + directNodeAttr +
            componentManagedAttributeMarker(parts, extras) +
            renderComponentAttributes(parts, extras) + attrs + '>' +
            escapeHtml(label) + '</button>' + children;
        }
        const tag = componentTagName(head);
        const parts = parseComponentElementParts(words, 1, scope);
        const contentExpr = parts.content || (node.expression || '').trim();
        const inline = children ? '' : (contentExpr
          ? escapeHtml(renderCompiledComponentExpression(parts.content ? parts.contentPlan : node.expressionPlan, contentExpr, scope))
          : '');
        const extras = semanticUiRenderExtras(head, parts);
        if (head === 'checkbox' && !componentPartsHaveAttribute(parts, 'type')) extras.type = 'checkbox';
        if ((head === 'file' || head === 'dropzone') && !componentPartsHaveAttribute(parts, 'type')) extras.type = 'file';
        extras['data-jtml-direct-node'] = head;
        return '<' + tag + directNodeAttr +
          componentManagedAttributeMarker(parts, extras) +
          renderComponentAttributes(parts, extras) + '>' +
          inline + children + '</' + tag + '>';
      }

      function renderComponentForRegion(definition, instance, node, scope, words) {
        return renderComponentForItems(definition, instance, node, scope, words)
          .map(function (item) { return item.wrapperHtml; })
          .join('');
      }

      function renderComponentForItems(definition, instance, node, scope, words) {
        words = words || splitComponentWords(node && node.text || '');
        const loopPlan = node && node.loopPlan || {};
        const raw = node.expression || words.slice(1).join(' ');
        const match = String(raw || '').match(/^([A-Za-z_][A-Za-z0-9_]*)\s+in\s+(.+)$/);
        const loopItemName = loopPlan.item || (match ? match[1] : '');
        const collectionExpr = loopPlan.collectionExpression || (match ? match[2] : raw);
        if (!loopItemName) return [];
        const result = evaluateCompiledComponentExpressionResult(
          loopPlan.collectionPlan || compileComponentExpressionPlan(collectionExpr),
          collectionExpr,
          scope);
        if (!result.found) return [];
        let values = result.value;
        if (values == null) values = [];
        if (!Array.isArray(values)) {
          if (typeof values === 'string') values = values.split('');
          else if (typeof values === 'object') values = Object.values(values);
          else values = [values];
        }
        const nodeIndex = (definition.bodyPlan || []).indexOf(node);
        const keySegments = values.map(function (item, itemIndex) {
          const childScope = Object.assign({}, scope);
          childScope[loopItemName] = item;
          let keySegment = String(itemIndex);
          if (node.keyExpression) {
            const keyResult = evaluateCompiledComponentExpressionResult(
              node.keyExpressionPlan || compileComponentExpressionPlan(node.keyExpression),
              node.keyExpression,
              childScope);
            if (keyResult.found) keySegment = componentPathSegment(renderTemplateValue(keyResult.value));
          }
          return keySegment;
        });
        recordDirectListLifecycle(instance, definition, node, nodeIndex, keySegments);
        return values.map(function (item, itemIndex) {
          const childScope = Object.assign({}, scope);
          childScope[loopItemName] = item;
          const keySegment = keySegments[itemIndex];
          componentRenderPath.push('for' + String(nodeIndex < 0 ? 'x' : nodeIndex) + '_' + keySegment);
          try {
            const innerHtml = renderComponentChildren(definition, instance, node, childScope);
            const wrapperHtml = '<span data-jtml-direct-list-item="' + String(nodeIndex < 0 ? 'x' : nodeIndex) + '"' +
              ' data-jtml-direct-list-key="' + escapeAttribute(keySegment) + '"' +
              ' data-jtml-direct-list-index="' + String(itemIndex) + '"' +
              ' style="display:contents">' +
              innerHtml +
              '</span>';
            return {
              key: String(keySegment),
              index: itemIndex,
              nodeIndex: nodeIndex,
              innerHtml: innerHtml,
              wrapperHtml: wrapperHtml
            };
          } finally {
            componentRenderPath.pop();
          }
        });
      }

      function recordDirectListLifecycle(instance, definition, node, nodeIndex, keys) {
        if (!instance) return;
        const regionKey = String(nodeIndex < 0 ? 'x' : nodeIndex);
        if (!instance.directListKeys || typeof instance.directListKeys !== 'object') {
          instance.directListKeys = {};
        }
        const previous = Array.isArray(instance.directListKeys[regionKey])
          ? instance.directListKeys[regionKey]
          : [];
        const next = (keys || []).map(function (key) { return String(key); });
        const previousIndex = {};
        const nextIndex = {};
        previous.forEach(function (key, index) {
          previousIndex[String(key)] = index;
        });
        next.forEach(function (key, index) {
          nextIndex[String(key)] = index;
        });
        const inserted = next.filter(function (key) {
          return !Object.prototype.hasOwnProperty.call(previousIndex, key);
        });
        const removed = previous.filter(function (key) {
          return !Object.prototype.hasOwnProperty.call(nextIndex, key);
        });
        const moved = next.filter(function (key, index) {
          return Object.prototype.hasOwnProperty.call(previousIndex, key) &&
            previousIndex[key] !== index;
        });
        instance.directListKeys[regionKey] = next.slice();
        if (!previous.length && !next.length) return;
        window.jtml = Object.assign(window.jtml || {}, {
          directComponentListLifecycle: {
            componentId: instance.id || '',
            component: instance.component || definition && definition.name || '',
            moduleId: instance.moduleId == null ? null : instance.moduleId,
            bodyNodeIndex: nodeIndex,
            bodySourceLine: node && node.sourceLine || 0,
            previousKeys: previous.slice(),
            currentKeys: next.slice(),
            insertedKeys: inserted,
            removedKeys: removed,
            movedKeys: moved,
            inserted: inserted.length,
            removed: removed.length,
            moved: moved.length,
            mode: 'keyed-list-markers'
          }
        });
      }

      function hasDuplicateStrings(values) {
        const seen = {};
        for (let i = 0; i < (values || []).length; i += 1) {
          const key = String(values[i]);
          if (Object.prototype.hasOwnProperty.call(seen, key)) return true;
          seen[key] = true;
        }
        return false;
      }

      function keyedListTemplateChildren(html) {
        const template = document.createElement('template');
        template.innerHTML = String(html || '');
        return Array.prototype.slice.call(template.content.childNodes || []);
      }

      function elementBodyNodeMarker(el) {
        return el && el.nodeType === 1 && el.getAttribute
          ? String(el.getAttribute('data-jtml-direct-body-node') || '')
          : '';
      }

      function samePatchableElementShape(current, next) {
        if (!current || !next || current.nodeType !== 1 || next.nodeType !== 1) return false;
        if (!current.tagName || !next.tagName ||
            current.tagName.toLowerCase() !== next.tagName.toLowerCase()) {
          return false;
        }
        const currentMarker = elementBodyNodeMarker(current);
        const nextMarker = elementBodyNodeMarker(next);
        return currentMarker && nextMarker && currentMarker === nextMarker;
      }

      function copyElementAttributesInPlace(current, next) {
        const seen = {};
        Array.prototype.slice.call(next.attributes || []).forEach(function (attr) {
          seen[attr.name] = true;
          if (current.getAttribute(attr.name) !== attr.value) {
            current.setAttribute(attr.name, attr.value);
          }
        });
        Array.prototype.slice.call(current.attributes || []).forEach(function (attr) {
          if (!seen[attr.name]) current.removeAttribute(attr.name);
        });
      }

      function hasNestedDirectComponentElement(el) {
        return !!(el && el.querySelector &&
          el.querySelector('[data-jtml-direct-instance]'));
      }

      function patchElementChildrenInPlace(current, next, stats) {
        const currentChildren = Array.prototype.slice.call(current.childNodes || []);
        const nextChildren = Array.prototype.slice.call(next.childNodes || []);
        if (currentChildren.length !== nextChildren.length) return false;
        for (let i = 0; i < nextChildren.length; i += 1) {
          const currentChild = currentChildren[i];
          const nextChild = nextChildren[i];
          if (!currentChild || !nextChild) return false;
          if (currentChild.nodeType === 3 && nextChild.nodeType === 3) {
            if (currentChild.nodeValue !== nextChild.nodeValue) {
              currentChild.nodeValue = nextChild.nodeValue;
              stats.textNodesPatched += 1;
            }
            continue;
          }
          if (samePatchableElementShape(currentChild, nextChild)) {
            if (!patchElementFromTemplateInPlace(currentChild, nextChild, stats)) {
              return false;
            }
            continue;
          }
          return false;
        }
        return true;
      }

      function patchElementFromTemplateInPlace(current, next, stats) {
        if (!samePatchableElementShape(current, next)) return false;
        copyElementAttributesInPlace(current, next);
        stats.elementsPatched += 1;
        if (hasNestedDirectComponentElement(current) || hasNestedDirectComponentElement(next)) {
          return current.innerHTML === next.innerHTML;
        }
        if (current.innerHTML === next.innerHTML) return true;
        if (patchElementChildrenInPlace(current, next, stats)) return true;
        return false;
      }

      function patchKeyedListItemWrapperInPlace(wrapper, item) {
        const stats = {
          changed: false,
          elementsPatched: 0,
          textNodesPatched: 0,
          childReplacements: 0,
          fullHtmlReplacements: 0
        };
        if (!wrapper) return stats;
        wrapper.setAttribute('data-jtml-direct-list-index', String(item.index));
        if (wrapper.innerHTML === item.innerHtml) return stats;
        const nextChildren = keyedListTemplateChildren(item.innerHtml);
        const currentChildren = Array.prototype.slice.call(wrapper.childNodes || []);
        if (nextChildren.length === currentChildren.length) {
          let patchedAll = true;
          for (let i = 0; i < nextChildren.length; i += 1) {
            const currentChild = currentChildren[i];
            const nextChild = nextChildren[i];
            if (currentChild && nextChild &&
                samePatchableElementShape(currentChild, nextChild) &&
                patchElementFromTemplateInPlace(currentChild, nextChild, stats)) {
              continue;
            }
            patchedAll = false;
            break;
          }
          if (patchedAll) {
            stats.changed = stats.elementsPatched > 0 || stats.textNodesPatched > 0;
            return stats;
          }
        }
        wrapper.innerHTML = item.innerHtml;
        stats.changed = true;
        stats.fullHtmlReplacements += 1;
        return stats;
      }

      function patchComponentForRegion(instance, definition, node, scope, el) {
        if (!node || componentNodeHead(node) !== 'for' || !el || !el.children) return false;
        const items = renderComponentForItems(definition, instance, node, scope);
        const nextKeys = items.map(function (item) { return item.key; });
        if (hasDuplicateStrings(nextKeys)) {
          recordCompiledPatchFallback(instance, definition, node, 'keyed for-region patch requires unique next keys', nextKeys);
          return false;
        }
        const nodeIndex = componentNodeIndex(definition, node);
        const itemMarker = String(nodeIndex < 0 ? 'x' : nodeIndex);
        const existing = Array.prototype.slice.call(el.children || []).filter(function (child) {
          return child && child.getAttribute &&
            child.getAttribute('data-jtml-direct-list-item') === itemMarker;
        });
        const previousKeys = existing.map(function (child) {
          return String(child.getAttribute('data-jtml-direct-list-key') || '');
        });
        const existingByKey = {};
        for (let i = 0; i < existing.length; i += 1) {
          const key = String(existing[i].getAttribute('data-jtml-direct-list-key') || '');
          if (Object.prototype.hasOwnProperty.call(existingByKey, key)) {
            recordCompiledPatchFallback(instance, definition, node, 'keyed for-region patch found duplicate existing keys', [key]);
            return false;
          }
          existingByKey[key] = existing[i];
        }
        const nextIndex = {};
        nextKeys.forEach(function (key, index) {
          nextIndex[String(key)] = index;
        });
        const previousIndex = {};
        previousKeys.forEach(function (key, index) {
          previousIndex[String(key)] = index;
        });
        const insertedKeys = nextKeys.filter(function (key) {
          return !Object.prototype.hasOwnProperty.call(previousIndex, key);
        });
        const removedKeys = previousKeys.filter(function (key) {
          return !Object.prototype.hasOwnProperty.call(nextIndex, key);
        });
        const retainedKeys = nextKeys.filter(function (key) {
          return Object.prototype.hasOwnProperty.call(previousIndex, key);
        });
        const movedKeys = retainedKeys.filter(function (key, index) {
          return previousIndex[key] !== nextIndex[key] || previousIndex[key] !== index;
        });
        let prunedDynamicInstances = 0;
        removedKeys.forEach(function (key) {
          prunedDynamicInstances += pruneDynamicComponentListItem(
            instance && instance.id || '',
            nodeIndex,
            key);
        });
        const fragment = document.createDocumentFragment();
        let reused = 0;
        let created = 0;
        let itemElementPatches = 0;
        let itemTextPatches = 0;
        let itemHtmlReplacements = 0;
        items.forEach(function (item) {
          let wrapper = existingByKey[item.key] || null;
          if (wrapper) {
            reused += 1;
            const patchStats = patchKeyedListItemWrapperInPlace(wrapper, item);
            itemElementPatches += patchStats.elementsPatched || 0;
            itemTextPatches += patchStats.textNodesPatched || 0;
            itemHtmlReplacements += patchStats.fullHtmlReplacements || 0;
          } else {
            created += 1;
            const template = document.createElement('template');
            template.innerHTML = item.wrapperHtml;
            wrapper = template.content.firstElementChild;
          }
          if (wrapper) fragment.appendChild(wrapper);
        });
        el.replaceChildren(fragment);
        window.jtml = Object.assign(window.jtml || {}, {
          directComponentKeyedListPatch: {
            componentId: instance && instance.id || '',
            component: instance && instance.component || definition && definition.name || '',
            bodyNodeIndex: nodeIndex,
            previousKeys: previousKeys,
            keys: nextKeys,
            retainedKeys: retainedKeys,
            insertedKeys: insertedKeys,
            removedKeys: removedKeys,
            movedKeys: movedKeys,
            reused: reused,
            created: created,
            removed: existing.length - reused,
            prunedDynamicInstances: prunedDynamicInstances,
            itemElementPatches: itemElementPatches,
            itemTextPatches: itemTextPatches,
            itemHtmlReplacements: itemHtmlReplacements,
            reordered: movedKeys.length > 0,
            mode: 'keyed-for-region-patch',
            reconciliation: 'below-wrapper'
          }
        });
        return true;
      }

      function componentPatchOperationKind(definition, node) {
        if (!node || node.kind !== 'template') return false;
        const head = componentNodeHead(node) || 'box';
        if (head === 'if' || head === 'for' || head === 'slot') return 'region';
        if (head === 'else' || head === 'while') return false;
        const nestedDefinition = componentDefinitionForName(
          head,
          node.definitionModule != null ? node.definitionModule : definition && definition.moduleId,
          definition && definition.moduleId);
        if (nestedDefinition) return 'nested-component';
        const hasChildren = !!(node.childIndices || []).length;
        if (hasChildren && head === 'button') return false;
        return hasChildren ? 'container-attrs' : 'leaf';
      }

      function componentPatchableNode(definition, node) {
        return !!componentPatchOperationKind(definition, node);
      }

      function componentUpdatePlanKey(definition) {
        const signature = (definition && definition.bodyPlan || []).map(function (node) {
          if (!node) return '';
          return [
            node.kind || '',
            node.name || '',
            node.text || '',
            node.expression || '',
            node.keyExpression || '',
            String(node.sourceLine || 0),
            (node.reads || []).join('|'),
            (node.writes || []).join('|'),
            (node.childIndices || []).join(',')
          ].join('#');
        }).join('@@');
        return String(definition && definition.moduleId != null ? definition.moduleId : 'global') +
          ':' + String(definition && definition.name || '') +
          ':' + String((definition && definition.bodyPlan || []).length) +
          ':' + signature;
      }

      function staticComponentUpdatePlanForKey(key) {
        const asset = window.__jtml_static_component_plan_index || window.__jtml_static_update_plans;
        const staticFunctions = window.__jtml_static_update_functions || {};
        const staticModules = window.__jtml_static_component_modules || {};
        const components = asset && Array.isArray(asset.components) ? asset.components : [];
        for (let i = 0; i < components.length; i += 1) {
          const candidate = components[i] || {};
          if (candidate.key !== key) continue;
          const staticUpdate = staticFunctions[key];
          const staticModule = staticModules[key] || {};
          return {
            key: key,
            component: candidate.name || '',
            moduleId: candidate.moduleId == null ? null : candidate.moduleId,
            entries: Array.isArray(candidate.entries) ? candidate.entries : [],
            entriesByRead: candidate.entriesByRead || {},
            unsafeEntries: Array.isArray(candidate.unsafeEntries) ? candidate.unsafeEntries : [],
            staticSource: true,
            staticCreateFunction: typeof staticModule.create === 'function' ? staticModule.create : null,
            staticUpdateFunction: typeof staticUpdate === 'function' ? staticUpdate : null,
            staticPatchCoverage: candidate.staticPatchCoverage || 'unknown'
          };
        }
        return null;
      }

      function compileComponentUpdatePlan(definition) {
        const key = componentUpdatePlanKey(definition);
        if (componentUpdatePlanCache[key]) return componentUpdatePlanCache[key];
        const staticPlan = staticComponentUpdatePlanForKey(key);
        if (staticPlan) {
          staticPlan.update = compileStaticComponentUpdateFunction(staticPlan) ||
            compileInterpretedComponentUpdateFunction(staticPlan);
          componentUpdatePlanCache[key] = staticPlan;
          return componentUpdatePlanCache[key];
        }
        const entries = [];
        const entriesByRead = {};
        const unsafeEntries = [];
        (definition && definition.bodyPlan || []).forEach(function (node, index) {
          if (!node || node.kind !== 'template' || isElseComponentNode(node)) return;
          const reads = componentNodeReadSet(definition, node);
          const readNames = Object.keys(reads);
          if (!componentPatchableNode(definition, node)) {
            unsafeEntries.push({
              index: index,
              reads: readNames,
              kind: componentNodeHead(node) || '',
              sourceLine: node && node.sourceLine || 0
            });
            return;
          }
          const operation = compileComponentPatchOperation(definition, node, index);
          if (!operation) return;
          const entry = {
            index: index,
            reads: readNames,
            kind: componentNodeHead(node) || '',
            sourceLine: node && node.sourceLine || 0,
            operation: operation
          };
          entries.push(entry);
          addComponentUpdateIndex(entriesByRead, entry);
        });
        const plan = {
          key: key,
          component: definition && definition.name || '',
          moduleId: definition && definition.moduleId != null ? definition.moduleId : null,
          entries: entries,
          entriesByRead: entriesByRead,
          unsafeEntries: unsafeEntries
        };
        plan.generatedSource = generateComponentUpdateFunctionSource(plan);
        plan.update = compileGeneratedComponentUpdateFunction(plan) ||
          compileInterpretedComponentUpdateFunction(plan);
        componentUpdatePlanCache[key] = plan;
        return componentUpdatePlanCache[key];
      }

      function addComponentUpdateIndex(index, entry) {
        (entry.reads || []).forEach(function (name) {
          if (!name) return;
          if (!index[name]) index[name] = [];
          index[name].push(entry);
        });
      }

      function compileComponentPatchOperation(definition, node, index) {
        if (!node || node.kind !== 'template') return null;
        const words = splitComponentWords(node.text || '');
        const head = componentNodeHead(node) || 'box';
        const patchKind = componentPatchOperationKind(definition, node);
        if (!patchKind) return null;
        const base = {
          nodeIndex: index,
          head: head,
          words: words,
          sourceLine: node && node.sourceLine || 0,
          expression: node.expression || ''
        };
        if (patchKind === 'region') {
          base.operation = 'region';
          return base;
        }
        if (patchKind === 'container-attrs') {
          base.operation = 'container-attrs';
          base.tag = componentTagName(head);
          base.partsPlan = compileComponentElementParts(words, 1);
          return base;
        }
        if (patchKind === 'nested-component') {
          base.operation = 'nested-component';
          return base;
        }
        if (head === 'show' || head === 'text') {
          base.operation = 'text';
          base.expression = node.expression || words.slice(1).join(' ');
          return base;
        }
        if (head === 'button') {
          base.operation = 'button';
          base.tag = 'button';
          base.partsPlan = compileComponentElementParts(words, 1, { click: true });
          base.clickIndex = words.indexOf('click');
          base.clickRaw = base.clickIndex === -1 ? '' : words[base.clickIndex + 1] || '';
          base.clickInvocation = compileComponentActionInvocation(base.clickRaw);
          return base;
        }
        base.operation = 'element';
        base.tag = componentTagName(head);
        base.partsPlan = compileComponentElementParts(words, 1);
        return base;
      }

      function componentPlanAffectedEntries(plan, changed) {
        const names = {};
        const seen = {};
        const affected = [];
        addComponentNameSet(names, changed || []);
        Object.keys(names).forEach(function (name) {
          (plan.entriesByRead && plan.entriesByRead[name] || []).forEach(function (entry) {
            const key = String(entry.index);
            if (seen[key]) return;
            seen[key] = true;
            affected.push(entry);
          });
        });
        return affected;
      }

      function firstUnsafeAffectedRenderNodeFromPlan(plan, definition, changed) {
        const names = {};
        addComponentNameSet(names, changed || []);
        const unsafe = plan && plan.unsafeEntries || [];
        for (let i = 0; i < unsafe.length; i += 1) {
          const entry = unsafe[i];
          if (!(entry.reads || []).some(function (name) { return !!names[name]; })) continue;
          return (definition.bodyPlan || [])[entry.index] || null;
        }
        return null;
      }

      function recordCompiledPatchFallback(instance, definition, node, reason, changed) {
        window.jtml = Object.assign(window.jtml || {}, {
          directComponentPatchFallback: {
            componentId: instance && instance.id || '',
            component: instance && instance.component || definition && definition.name || '',
            reason: reason || 'compiled patch plan could not handle update',
            writes: changed || [],
            bodySourceLine: node && node.sourceLine || 0,
            bodyNodeHead: node ? componentNodeHead(node) : '',
            bodyNodeKind: node && node.kind || ''
          }
        });
      }

      function recordComponentUpdatePlanSuccess(instance, definition, plan, changed, patched, mode) {
        window.jtml = Object.assign(window.jtml || {}, {
          directComponentCompiledUpdatePlan: {
            componentId: instance && instance.id || '',
            component: instance && instance.component || definition && definition.name || '',
            planKey: plan && plan.key || '',
            writes: changed,
            patchedNodes: patched,
            operationCount: plan && plan.__lastOperationCount || 0,
            indexedReadCount: Object.keys(plan && plan.entriesByRead || {}).length,
            unsafeEntryCount: (plan && plan.unsafeEntries || []).length,
            generatedSourceLength: String(plan && plan.generatedSource || '').length,
            mode: mode || 'indexed-compiled-update-function'
          }
        });
      }

      function generatedUpdateHelpers() {
        return {
          componentPlanAffectedEntries: componentPlanAffectedEntries,
          firstUnsafeAffectedRenderNodeFromPlan: firstUnsafeAffectedRenderNodeFromPlan,
          executeComponentPatchOperation: executeComponentPatchOperation,
          replaceDirectComponentNode: replaceDirectComponentNode,
          recordCompiledPatchFallback: recordCompiledPatchFallback,
          recordComponentUpdatePlanSuccess: recordComponentUpdatePlanSuccess,
          patchStaticComponentButtonNode: patchStaticComponentButtonNode,
          patchStaticComponentElementNode: patchStaticComponentElementNode,
          renderStaticComponentButtonNode: renderStaticComponentButtonNode,
          renderStaticComponentCreateOperations: renderStaticComponentCreateOperations,
          renderStaticComponentElementNode: renderStaticComponentElementNode,
          patchStaticComponentNodeHtml: patchStaticComponentNodeHtml,
          patchStaticComponentRegionNode: patchStaticComponentRegionNode,
          patchStaticComponentTextNode: patchStaticComponentTextNode,
          renderStaticComponentRegionNode: renderStaticComponentRegionNode,
          renderStaticComponentTextNode: renderStaticComponentTextNode
        };
      }

      function renderStaticComponentRoots(instance, definition, scope) {
        const roots = (definition && definition.bodyPlan || []).filter(function (node) {
          return node && node.renderRoot && node.kind === 'template';
        });
        return roots.map(function (node) {
          return renderComponentPlanNode(definition, instance, node, scope);
        }).join('');
      }

      function renderStaticComponentCreateOperation(instance, definition, scope, operation) {
        if (!operation || operation.nodeIndex == null) return null;
        const node = (definition && definition.bodyPlan || [])[operation.nodeIndex];
        if (!node) return null;
        const head = operation.head || componentNodeHead(node) || 'box';
        if (operation.operation === 'region' ||
            operation.operation === 'nested-component') {
          return renderStaticComponentRegionNode(instance, definition, scope, operation.nodeIndex);
        }
        const words = Array.isArray(operation.words) && operation.words.length
          ? operation.words
          : splitComponentWords(node.text || '');
        if (operation.operation === 'text') {
          return renderStaticComponentTextNode(
            instance,
            definition,
            scope,
            operation.nodeIndex,
            operation.expression || words.slice(1).join(' '),
            operation.expressionPlan || null);
        }
        if (operation.operation === 'button') {
          return renderStaticComponentButtonNode(
            instance,
            definition,
            scope,
            operation.nodeIndex,
            head,
            operation.partsPlan || compileComponentElementParts(words, 1, { click: true }),
            operation.clickInvocation || null);
        }
        if (operation.operation === 'element' ||
            operation.operation === 'container-attrs') {
          return renderStaticComponentElementNode(
            instance,
            definition,
            scope,
            operation.nodeIndex,
            head,
            operation.tag || componentTagName(head),
            operation.partsPlan || compileComponentElementParts(words, 1),
            operation.expression || '',
            operation.expressionPlan || null);
        }
        return null;
      }

      function renderStaticComponentRegionNode(instance, definition, scope, nodeIndex) {
        const node = (definition && definition.bodyPlan || [])[nodeIndex];
        if (!node) return null;
        return renderComponentPlanNode(definition, instance, node, scope);
      }

      function renderStaticComponentTextNode(instance, definition, scope, nodeIndex, expression, expressionPlan) {
        const node = (definition && definition.bodyPlan || [])[nodeIndex];
        if (!node) return null;
        const expr = expression || node.expression || splitComponentWords(node.text || '').slice(1).join(' ');
        const directNodeAttr = ' data-jtml-direct-body-node="' + String(nodeIndex) + '"';
        return '<p' + directNodeAttr + ' data-jtml-direct-text-node="true">' +
          escapeHtml(renderCompiledComponentExpression(expressionPlan, expr, scope)) +
          '</p>';
      }

      function renderStaticComponentButtonNode(instance, definition, scope, nodeIndex, head, partsPlan, clickInvocation) {
        const node = (definition && definition.bodyPlan || [])[nodeIndex];
        if (!node) return null;
        const words = splitComponentWords(node.text || '');
        const resolvedHead = head || componentNodeHead(node) || 'button';
        const parts = evaluateCompiledComponentElementParts(
          partsPlan || compileComponentElementParts(words, 1, { click: true }),
          scope);
        const clickIndex = words.indexOf('click');
        const invocation = clickInvocation && clickInvocation.name
          ? evaluateCompiledComponentActionInvocation(clickInvocation, scope)
          : clickIndex === -1
          ? { name: '', args: [] }
          : parseComponentActionInvocation(words[clickIndex + 1] || '', scope);
        const label = renderCompiledComponentExpression(parts.contentPlan, parts.content || '"Button"', scope);
        const directNodeAttr = ' data-jtml-direct-body-node="' + String(nodeIndex) + '"';
        const attrs = invocation.name
          ? ' data-jtml-direct-component-id="' + escapeHtml(instance.id) + '"' +
            ' data-jtml-direct-component-action="' + escapeHtml(invocation.name) + '"' +
            ' data-jtml-direct-component-args="' + escapeAttribute(JSON.stringify(invocation.args || [])) + '"'
          : '';
        const extras = semanticUiRenderExtras(resolvedHead, parts);
        return '<button type="button"' + directNodeAttr +
          componentManagedAttributeMarker(parts, extras) +
          renderComponentAttributes(parts, extras) + attrs + '>' +
          escapeHtml(label) + '</button>';
      }

      function renderStaticComponentElementNode(instance, definition, scope, nodeIndex, head, tag, partsPlan, expression, expressionPlan) {
        const node = (definition && definition.bodyPlan || [])[nodeIndex];
        if (!node) return null;
        const resolvedHead = head || componentNodeHead(node) || 'box';
        const resolvedTag = tag || componentTagName(resolvedHead);
        const words = splitComponentWords(node.text || '');
        const parts = evaluateCompiledComponentElementParts(
          partsPlan || compileComponentElementParts(words, 1),
          scope);
        const children = renderComponentChildren(definition, instance, node, scope);
        const contentExpr = parts.content || String(expression || node.expression || '').trim();
        const inline = children
          ? ''
          : (contentExpr
              ? escapeHtml(renderCompiledComponentExpression(parts.content ? parts.contentPlan : expressionPlan, contentExpr, scope))
              : '');
        const extras = semanticUiRenderExtras(resolvedHead, parts);
        if (resolvedHead === 'checkbox' && !componentPartsHaveAttribute(parts, 'type')) extras.type = 'checkbox';
        if ((resolvedHead === 'file' || resolvedHead === 'dropzone') && !componentPartsHaveAttribute(parts, 'type')) extras.type = 'file';
        extras['data-jtml-direct-node'] = resolvedHead;
        const directNodeAttr = ' data-jtml-direct-body-node="' + String(nodeIndex) + '"';
        return '<' + resolvedTag + directNodeAttr +
          componentManagedAttributeMarker(parts, extras) +
          renderComponentAttributes(parts, extras) + '>' +
          inline + children + '</' + resolvedTag + '>';
      }

      function renderStaticComponentCreateOperations(instance, definition, scope, operations) {
        const rendered = [];
        const list = Array.isArray(operations) ? operations : [];
        for (let i = 0; i < list.length; i += 1) {
          const html = renderStaticComponentCreateOperation(instance, definition, scope, list[i]);
          if (typeof html !== 'string') return null;
          rendered.push(html);
        }
        window.jtml = Object.assign(window.jtml || {}, {
          directComponentStaticCreateOperations: {
            componentId: instance && instance.id || '',
            component: instance && instance.component || definition && definition.name || '',
            operationCount: list.length,
            mode: 'static-create-operations'
          }
        });
        return rendered.join('');
      }

      function generateComponentUpdateFunctionSource(plan) {
        const serialized = JSON.stringify({
          key: plan && plan.key || '',
          component: plan && plan.component || '',
          moduleId: plan && plan.moduleId == null ? null : plan.moduleId,
          entries: plan && Array.isArray(plan.entries) ? plan.entries : [],
          entriesByRead: plan && plan.entriesByRead || {},
          unsafeEntries: plan && Array.isArray(plan.unsafeEntries) ? plan.unsafeEntries : []
        }).replace(/<\//g, '<\\/');
        return [
          '"use strict";',
          'const plan = ' + serialized + ';',
          'return function generatedJtmlComponentUpdate(instance, definition, changed, scope) {',
          '  const affected = h.componentPlanAffectedEntries(plan, changed);',
          '  plan.__lastOperationCount = affected.length;',
          '  if (!affected.length) return { handled: false, patched: 0 };',
          '  const unsafeNode = h.firstUnsafeAffectedRenderNodeFromPlan(plan, definition, changed);',
          '  if (unsafeNode) {',
          '    h.recordCompiledPatchFallback(instance, definition, unsafeNode, "affected body-plan node requires full rerender", changed);',
          '    return { handled: false, patched: 0 };',
          '  }',
          '  let patched = 0;',
          '  for (let i = 0; i < affected.length; i += 1) {',
          '    const node = (definition.bodyPlan || [])[affected[i].index];',
          '    if (!node) return { handled: false, patched: patched };',
          '    if (!h.executeComponentPatchOperation(instance, definition, affected[i].operation, scope) &&',
          '        !h.replaceDirectComponentNode(instance, definition, node, scope)) {',
          '      h.recordCompiledPatchFallback(instance, definition, node, "generated patch operation failed for affected body-plan node", changed);',
          '      return { handled: false, patched: patched };',
          '    }',
          '    patched += 1;',
          '  }',
          '  h.recordComponentUpdatePlanSuccess(instance, definition, plan, changed, patched, "generated-production-update-function");',
          '  return { handled: true, patched: patched };',
          '};'
        ].join('\n');
      }

      function compileGeneratedComponentUpdateFunction(plan) {
        if (!plan || !plan.generatedSource) return null;
        if (!dynamicGeneratedUpdateFunctions) {
          window.jtml = Object.assign(window.jtml || {}, {
            directComponentGeneratedUpdatePolicy: {
              component: plan && plan.component || '',
              planKey: plan && plan.key || '',
              generatedSourceLength: String(plan && plan.generatedSource || '').length,
              dynamicExecution: false,
              reason: 'dynamic generated update functions disabled; using CSP-safe interpreted update plan'
            }
          });
          return null;
        }
        if (typeof Function !== 'function') return null;
        try {
          const factory = new Function('h', plan.generatedSource);
          const update = factory(generatedUpdateHelpers());
          if (typeof update !== 'function') return null;
          update.__jtmlGenerated = true;
          return update;
        } catch (err) {
          window.jtml = Object.assign(window.jtml || {}, {
            directComponentGeneratedUpdateError: {
              component: plan && plan.component || '',
              planKey: plan && plan.key || '',
              message: err && err.message ? err.message : 'generated update function unavailable'
            }
          });
          return null;
        }
      }

      function compileStaticComponentUpdateFunction(plan) {
        if (!plan || typeof plan.staticUpdateFunction !== 'function') return null;
        return function (instance, definition, changed, scope) {
          try {
            const result = plan.staticUpdateFunction(
              instance,
              definition,
              changed,
              scope,
              generatedUpdateHelpers());
            if (result && result.handled) return result;
            return result || { handled: false, patched: 0 };
          } catch (err) {
            window.jtml = Object.assign(window.jtml || {}, {
              directComponentStaticUpdateError: {
                component: plan && plan.component || '',
                planKey: plan && plan.key || '',
                message: err && err.message ? err.message : 'static update function unavailable'
              }
            });
            return { handled: false, patched: 0 };
          }
        };
      }

      function compileInterpretedComponentUpdateFunction(plan) {
        return function (instance, definition, changed, scope) {
          const affected = componentPlanAffectedEntries(plan, changed);
          plan.__lastOperationCount = affected.length;
          if (!affected.length) return { handled: false, patched: 0 };
          const unsafeNode = firstUnsafeAffectedRenderNodeFromPlan(plan, definition, changed);
          if (unsafeNode) {
            recordCompiledPatchFallback(instance, definition, unsafeNode, 'affected body-plan node requires full rerender', changed);
            return { handled: false, patched: 0 };
          }
          let patched = 0;
          for (let i = 0; i < affected.length; i += 1) {
            const node = (definition.bodyPlan || [])[affected[i].index];
            if (!node) return { handled: false, patched: patched };
            if (!executeComponentPatchOperation(instance, definition, affected[i].operation, scope) &&
                !replaceDirectComponentNode(instance, definition, node, scope)) {
              recordCompiledPatchFallback(instance, definition, node, 'patch operation failed for affected body-plan node', changed);
              return { handled: false, patched: patched };
            }
            patched += 1;
          }
          recordComponentUpdatePlanSuccess(
            instance,
            definition,
            plan,
            changed,
            patched,
            plan && plan.staticSource
              ? 'static-update-plan-interpreter'
              : 'indexed-compiled-update-function');
          return { handled: true, patched: patched };
        };
      }

      function replaceDirectComponentNode(instance, definition, node, scope) {
        const index = componentNodeIndex(definition, node);
        if (index < 0 || !instance || !instance.element) return false;
        const html = renderComponentPlanNode(definition, instance, node, scope);
        return patchStaticComponentNodeHtml(instance, index, html);
      }

      function patchStaticComponentNodeHtml(instance, nodeIndex, html) {
        if (!instance || !instance.element) return false;
        const el = instance.element.querySelector('[data-jtml-direct-body-node="' + String(nodeIndex) + '"]');
        if (!el) return false;
        if (typeof html !== 'string' || !html.length) return false;
        el.outerHTML = html;
        return true;
      }

      function directComponentElementForNode(instance, nodeIndex) {
        if (!instance || !instance.element) return null;
        return instance.element.querySelector('[data-jtml-direct-body-node="' + String(nodeIndex) + '"]');
      }

      function patchStaticComponentTextNode(instance, nodeIndex, expression, scope, expressionPlan) {
        const el = directComponentElementForNode(instance, nodeIndex);
        if (!el) return false;
        el.textContent = renderCompiledComponentExpression(expressionPlan, expression || '', scope);
        return true;
      }

      function patchStaticComponentRegionNode(instance, definition, nodeIndex, scope) {
        const node = (definition && definition.bodyPlan || [])[nodeIndex];
        const el = directComponentElementForNode(instance, nodeIndex);
        if (!node || !el) return false;
        if (componentNodeHead(node) === 'for' &&
            patchComponentForRegion(instance, definition, node, scope, el)) {
          return true;
        }
        const html = renderComponentPlanNode(definition, instance, node, scope);
        if (typeof html !== 'string') return false;
        el.outerHTML = html;
        return true;
      }

      function patchStaticComponentButtonNode(instance, nodeIndex, head, partsPlan, clickInvocation, scope) {
        const el = directComponentElementForNode(instance, nodeIndex);
        if (!el || !el.tagName || el.tagName.toLowerCase() !== 'button') return false;
        const resolvedHead = head || 'button';
        const parts = evaluateCompiledComponentElementParts(partsPlan, scope);
        const invocation = clickInvocation && clickInvocation.name
          ? evaluateCompiledComponentActionInvocation(clickInvocation, scope)
          : { name: '', args: [] };
        const label = renderCompiledComponentExpression(parts.contentPlan, parts.content || '"Button"', scope);
        if (el.textContent !== label) el.textContent = label;
        if (invocation.name) {
          el.setAttribute('data-jtml-direct-component-id', instance.id || '');
          el.setAttribute('data-jtml-direct-component-action', invocation.name);
          el.setAttribute('data-jtml-direct-component-args', JSON.stringify(invocation.args || []));
        }
        return patchDirectComponentElementAttributes(el, parts, semanticUiRenderExtras(resolvedHead, parts));
      }

      function patchStaticComponentElementNode(instance, nodeIndex, head, tag, partsPlan, expression, scope, patchContent, expressionPlan) {
        const el = directComponentElementForNode(instance, nodeIndex);
        const resolvedHead = head || 'box';
        const expectedTag = tag || componentTagName(resolvedHead);
        if (!el || !el.tagName || el.tagName.toLowerCase() !== expectedTag.toLowerCase()) return false;
        const parts = evaluateCompiledComponentElementParts(partsPlan, scope);
        const contentExpr = parts.content || String(expression || '').trim();
        if (patchContent && contentExpr) {
          const label = renderCompiledComponentExpression(parts.content ? parts.contentPlan : expressionPlan, contentExpr, scope);
          if (el.textContent !== label) el.textContent = label;
        }
        const extras = semanticUiRenderExtras(resolvedHead, parts);
        if (resolvedHead === 'checkbox' && !componentPartsHaveAttribute(parts, 'type')) extras.type = 'checkbox';
        if ((resolvedHead === 'file' || resolvedHead === 'dropzone') && !componentPartsHaveAttribute(parts, 'type')) extras.type = 'file';
        extras['data-jtml-direct-node'] = resolvedHead;
        return patchDirectComponentElementAttributes(el, parts, extras);
      }

      function executeComponentPatchOperation(instance, definition, operation, scope) {
        if (!operation || !instance || !instance.element) return false;
        const el = instance.element.querySelector('[data-jtml-direct-body-node="' + String(operation.nodeIndex) + '"]');
        if (!el) return false;
        if (operation.operation === 'text') {
          return patchStaticComponentTextNode(
            instance,
            operation.nodeIndex,
            operation.expression || '',
            scope,
            operation.expressionPlan || null);
        }
        if (operation.operation === 'region') {
          return patchStaticComponentRegionNode(instance, definition, operation.nodeIndex, scope);
        }
        const expectedTag = operation.tag || componentTagName(operation.head || 'box');
        if (el.tagName && el.tagName.toLowerCase() !== expectedTag.toLowerCase()) return false;
        if (operation.operation === 'button') {
          return patchStaticComponentButtonNode(
            instance,
            operation.nodeIndex,
            operation.head,
            operation.partsPlan,
            operation.clickInvocation,
            scope);
        }
        if (operation.operation === 'container-attrs') {
          return patchStaticComponentElementNode(
            instance,
            operation.nodeIndex,
            operation.head,
            expectedTag,
            operation.partsPlan,
            '',
            scope,
            false,
            operation.expressionPlan || null);
        }
        if (operation.operation === 'nested-component') {
          return patchStaticComponentRegionNode(instance, definition, operation.nodeIndex, scope);
        }
        if (operation.operation !== 'element') return false;
        return patchStaticComponentElementNode(
          instance,
          operation.nodeIndex,
          operation.head,
          expectedTag,
          operation.partsPlan,
          operation.expression,
          scope,
          true,
          operation.expressionPlan || null);
      }

      function patchDirectComponentElementAttributes(el, parts, extra) {
        const previous = String(el.getAttribute('data-jtml-direct-managed-attrs') || '')
          .split(',')
          .filter(Boolean);
        const next = componentAttributeMap(parts, extra);
        previous.forEach(function (name) {
          if (!Object.prototype.hasOwnProperty.call(next, name)) {
            el.removeAttribute(name);
          }
        });
        Object.keys(next).forEach(function (name) {
          applyAttribute(el, name, next[name]);
        });
        el.setAttribute('data-jtml-direct-managed-attrs', Object.keys(next).join(','));
        return true;
      }

      function applyCompiledComponentUpdatePlan(instance, definition, changed, scope) {
        const plan = compileComponentUpdatePlan(definition);
        if (!plan || typeof plan.update !== 'function') return { handled: false, patched: 0 };
        return plan.update(instance, definition, changed, scope);
      }

      function patchDirectComponentFromWrites(instance, definition, writes) {
        if (!instance || !instance.element || !definition) return false;
        const changed = Object.keys(writes || {});
        if (!changed.length) return false;
        const scope = componentScopeFor(instance, definition);
        applyComponentDerived(scope, definition);
        instance.scope = scope;
        const compiled = applyCompiledComponentUpdatePlan(instance, definition, changed, scope);
        if (compiled.handled) {
          if (compiled.patched) {
            window.jtml = Object.assign(window.jtml || {}, {
              directComponentPatchCount: (window.jtml && window.jtml.directComponentPatchCount || 0) + compiled.patched,
              directComponentLastPatch: {
                componentId: instance.id || '',
                component: instance.component || definition.name || '',
                writes: changed,
                patchedNodes: compiled.patched,
                mode: 'compiled-update-plan'
              }
            });
          }
          return compiled.patched > 0;
        }
        recordCompiledPatchFallback(instance, definition, null, 'compiled body-plan update unavailable; full direct rerender required', changed);
        return false;
      }

      function evaluateComponentBodyNodeExpression(node, fallbackExpression, scope) {
        const expr = fallbackExpression != null
          ? String(fallbackExpression || '')
          : String(node && node.expression || '');
        const compiled = evaluateCompiledComponentExpressionResult(
          node && node.expressionPlan || compileComponentExpressionPlan(expr),
          expr,
          scope);
        if (compiled && compiled.found) return compiled.value;
        const result = evaluateClientBodyExpression(expr, scope);
        return result && result.found ? result.value : unquoteComponentText(expr);
      }

      function applyComponentAssignment(scope, node) {
        if (!node || node.kind !== 'assignment' || !node.name) return false;
        const next = evaluateComponentBodyNodeExpression(node, node.expression || '', scope);
        const targetParts = parseClientPathSegments(node.name, scope);
        if (!targetParts.length) return false;
        const isPathAssignment = targetParts.length > 1;
        const currentResult = isPathAssignment ? deepGet(scope, node.name) : { found: true, value: scope[node.name] };
        const current = currentResult.found ? currentResult.value : undefined;
        const op = node.operator || '=';
        let assigned = next;
        if (op === '=') assigned = next;
        else if (op === '+=') {
          if (typeof current === 'number' && typeof next === 'number') {
            assigned = current + next;
          } else if (Number.isFinite(Number(current)) && Number.isFinite(Number(next))) {
            assigned = Number(current) + Number(next);
          } else {
            assigned = String(current == null ? '' : current) + String(next == null ? '' : next);
          }
        } else if (op === '-=') assigned = Number(current || 0) - Number(next);
        else if (op === '*=') assigned = Number(current || 0) * Number(next);
        else if (op === '/=') assigned = Number(current || 0) / Number(next);
        else if (op === '%=') assigned = Number(current || 0) % Number(next);
        else return false;
        if (isPathAssignment) {
          if (!Object.prototype.hasOwnProperty.call(scope, targetParts[0]) ||
              scope[targetParts[0]] == null) {
            scope[targetParts[0]] = typeof targetParts[1] === 'number' ? [] : {};
          }
          return assignObjectPath(scope[targetParts[0]], targetParts.slice(1), assigned);
        }
        scope[node.name] = assigned;
        return true;
      }

      function componentActionArgsFromExpression(expression, scope) {
        const args = [];
        const source = String(expression || '').trim();
        if (!source) return args;
        splitTopLevelList(source).forEach(function (expr) {
          const plan = compileComponentExpressionPlan(expr);
          const compiled = evaluateCompiledComponentExpressionResult(plan, expr, scope);
          args.push(compiled && compiled.found ? compiled.value : evaluateComponentValue(expr, scope));
        });
        return args;
      }

      function applyComponentStateDeclaration(scope, node) {
        if (!node || !node.name) return false;
        if ((node.childIndices || []).length) return false;
        scope[node.name] = evaluateComponentBodyNodeExpression(node, node.expression || '', scope);
        return true;
      }

      function runComponentPlanStatements(definition, scope, nodes, instance, changes, actionContext) {
        const list = nodes || [];
        for (let i = 0; i < list.length; i += 1) {
          const node = list[i];
          if (!node) continue;
          const head = componentNodeHead(node);
          if (isElseComponentNode(node)) continue;
          if (node.kind === 'state' || node.kind === 'derived') {
            if (!applyComponentStateDeclaration(scope, node)) {
              return recordComponentBodyPlanFallback(instance, definition, node, 'unsupported local declaration', actionContext);
            }
            addComponentNameSet(changes && changes.writes || {}, node.writes && node.writes.length ? node.writes : [node.name]);
            continue;
          }
          if (node.kind === 'assignment') {
            if (!applyComponentAssignment(scope, node)) {
              return recordComponentBodyPlanFallback(instance, definition, node, 'unsupported assignment', actionContext);
            }
            if ((node.childIndices || []).length) {
              return recordComponentBodyPlanFallback(instance, definition, node, 'assignment nodes cannot have child statements', actionContext);
            }
            addComponentNameSet(changes && changes.writes || {}, node.writes && node.writes.length ? node.writes : [node.name]);
            continue;
          }
          if (node.kind === 'call' && node.name) {
            const callArgs = componentActionArgsFromExpression(node.expression || '', scope);
            const actionNode = (definition.bodyPlan || []).find(function (candidate) {
              return candidate && candidate.kind === 'action' && candidate.name === node.name;
            });
            if (!actionNode) {
              if (instance && emitComponentEvent(instance, node.name, callArgs)) continue;
              return recordComponentBodyPlanFallback(instance, definition, node, 'call target is not a local action or declared emitted event', actionContext);
            }
            const nestedActionContext = {
              name: actionNode.name || node.name || '',
              sourceLine: actionNode.sourceLine || 0,
              text: actionNode.text || ''
            };
            const actionWords = splitComponentWords(actionNode.text || '');
            const previous = {};
            const hadPrevious = {};
            (actionWords.slice(2) || []).forEach(function (param, index) {
              if (!param) return;
              hadPrevious[param] = Object.prototype.hasOwnProperty.call(scope, param);
              previous[param] = scope[param];
              scope[param] = callArgs[index];
            });
            const nestedOk = runComponentPlanStatements(
                definition,
                scope,
                componentNodeChildren(definition, actionNode),
                instance,
                changes,
                nestedActionContext);
            (actionWords.slice(2) || []).forEach(function (param) {
              if (!param) return;
              if (hadPrevious[param]) scope[param] = previous[param];
              else delete scope[param];
            });
            if (!nestedOk) return false;
            continue;
          }
          if (node.kind === 'template' && head === 'if') {
            const words = splitComponentWords(node.text || '');
            const conditionExpr = node.expression || words.slice(1).join(' ');
            const condition = evaluateCompiledComponentExpressionResult(
              node.expressionPlan || compileComponentExpressionPlan(conditionExpr),
              conditionExpr,
              scope);
            if (condition.found && condition.value) {
              if (!runComponentPlanStatements(definition, scope, componentNodeChildren(definition, node), instance, changes, actionContext)) return false;
            } else {
              const elseNode = componentElseSibling(definition, node);
              if (elseNode &&
                  !runComponentPlanStatements(definition, scope, componentNodeChildren(definition, elseNode), instance, changes, actionContext)) {
                return false;
              }
            }
            continue;
          }
          if (node.kind === 'template' && head === 'for') {
            const words = splitComponentWords(node.text || '');
            const raw = node.expression || words.slice(1).join(' ');
            const match = String(raw || '').match(/^([A-Za-z_][A-Za-z0-9_]*)\s+in\s+(.+)$/);
            const loopPlan = node && node.loopPlan || {};
            const loopItemName = loopPlan.item || (match ? match[1] : '');
            const collectionExpr = loopPlan.collectionExpression || (match ? match[2] : raw);
            if (!loopItemName) {
              return recordComponentBodyPlanFallback(instance, definition, node, 'for loop must use `item in collection`', actionContext);
            }
            const result = evaluateCompiledComponentExpressionResult(
              loopPlan.collectionPlan || compileComponentExpressionPlan(collectionExpr),
              collectionExpr,
              scope);
            if (!result.found) {
              return recordComponentBodyPlanFallback(instance, definition, node, 'for loop collection could not be evaluated', actionContext);
            }
            let values = result.value;
            if (values == null) values = [];
            if (!Array.isArray(values)) {
              if (typeof values === 'string') values = values.split('');
              else if (typeof values === 'object') values = Object.values(values);
              else values = [values];
            }
            const previous = Object.prototype.hasOwnProperty.call(scope, loopItemName)
              ? scope[loopItemName]
              : undefined;
            const hadPrevious = Object.prototype.hasOwnProperty.call(scope, loopItemName);
            for (let itemIndex = 0; itemIndex < values.length; itemIndex += 1) {
              scope[loopItemName] = values[itemIndex];
              if (!runComponentPlanStatements(definition, scope, componentNodeChildren(definition, node), instance, changes, actionContext)) return false;
            }
            if (hadPrevious) scope[loopItemName] = previous;
            else delete scope[loopItemName];
            continue;
          }
          if (node.kind === 'template' && head === 'while') {
            const words = splitComponentWords(node.text || '');
            const conditionExpr = node.expression || words.slice(1).join(' ');
            let guard = 0;
            while (true) {
              const condition = evaluateCompiledComponentExpressionResult(
                node.expressionPlan || compileComponentExpressionPlan(conditionExpr),
                conditionExpr,
                scope);
              if (!condition.found) {
                return recordComponentBodyPlanFallback(instance, definition, node, 'while condition could not be evaluated', actionContext);
              }
              if (!condition.value) break;
              if (guard++ > 10000) {
                return recordComponentBodyPlanFallback(instance, definition, node, 'while loop exceeded direct-execution guard', actionContext);
              }
              if (!runComponentPlanStatements(definition, scope, componentNodeChildren(definition, node), instance, changes, actionContext)) return false;
            }
            continue;
          }
          return recordComponentBodyPlanFallback(instance, definition, node, 'unsupported statement kind `' + String(node.kind || '') + '`', actionContext);
        }
        return true;
      }

      function renderDirectComponent(instance) {
        if (!instance || !instance.element) return false;
        const definition = componentDefinitionForInstance(instance);
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
        const renderedDynamicIds = new Set();
        dynamicComponentRenderStack.push(renderedDynamicIds);
        let html = '';
        try {
          const plan = compileComponentUpdatePlan(definition);
          if (plan && typeof plan.staticCreateFunction === 'function') {
            html = plan.staticCreateFunction(
              instance,
              definition,
              scope,
              Object.assign(generatedUpdateHelpers(), {
                renderStaticComponentRoots: renderStaticComponentRoots
              }));
            window.jtml = Object.assign(window.jtml || {}, {
              directComponentStaticCreate: {
                componentId: instance.id || '',
                component: instance.component || definition.name || '',
                planKey: plan.key || '',
                mode: plan.__lastCreateMode || 'static-production-create-function',
                directCreateCount: plan.__lastDirectCreateCount || 0
              }
            });
          } else {
            html = renderStaticComponentRoots(instance, definition, scope);
          }
        } finally {
          dynamicComponentRenderStack.pop();
        }
        pruneDynamicComponentSubtree(instance.id, renderedDynamicIds);
        instance.element.innerHTML = html;
        instance.element.dataset.jtmlDirectRendered = 'true';
        return true;
      }

      function renderDirectComponents() {
        if (!browserLocalRuntime) return;
        let rendered = 0;
        componentInstances.forEach(function (instance) {
          if (renderDirectComponent(instance)) rendered += 1;
        });
        window.jtml = Object.assign(window.jtml || {}, {
          directComponentExecution: true,
          directComponentRenderCount: rendered
        });
      }

      function componentEventPayloadTypeMatches(value, declaredType) {
        const type = String(declaredType || '').trim().toLowerCase();
        if (!type || type === 'any' || type === 'unknown') return true;
        if (type === 'string') return typeof value === 'string';
        if (type === 'number' || type === 'float' || type === 'double' || type === 'int') {
          return typeof value === 'number' && Number.isFinite(value);
        }
        if (type === 'boolean' || type === 'bool') return typeof value === 'boolean';
        if (type === 'array' || type.endsWith('[]')) return Array.isArray(value);
        if (type === 'object' || type === 'dict' || type === 'record') {
          return value !== null && typeof value === 'object' && !Array.isArray(value);
        }
        if (type === 'null') return value === null;
        return true;
      }

      function emitComponentEvent(instance, eventName, args) {
        if (!instance || !eventName) return false;
        const definition = componentDefinitionForInstance(instance);
        const declaredEmits = definition && Array.isArray(definition.emits) ? definition.emits : [];
        if (declaredEmits.length && declaredEmits.indexOf(eventName) === -1) return false;
        const arityMap = definition && definition.emitArity && typeof definition.emitArity === 'object'
          ? definition.emitArity
          : {};
        if (Object.prototype.hasOwnProperty.call(arityMap, eventName)) {
          const forwardedCount = Array.isArray(args) ? args.length : 0;
          if (forwardedCount !== Number(arityMap[eventName] || 0)) return false;
        }
        const typeMap = definition && definition.emitPayloadTypes && typeof definition.emitPayloadTypes === 'object'
          ? definition.emitPayloadTypes
          : {};
        if (Object.prototype.hasOwnProperty.call(typeMap, eventName)) {
          const forwardedArgs = Array.isArray(args) ? args : [];
          const declaredTypes = Array.isArray(typeMap[eventName]) ? typeMap[eventName] : [];
          for (let i = 0; i < declaredTypes.length; i += 1) {
            if (!componentEventPayloadTypeMatches(forwardedArgs[i], declaredTypes[i])) return false;
          }
        }
        const handlers = instance.eventHandlers || {};
        const handler = handlers[eventName];
        if (!handler || !handler.name || !instance.parentId) return false;
        const parent = findRuntimeComponentInstance(instance.parentId);
        if (!parent) return false;
        const forwardedArgs = Array.isArray(args) ? args : [];
        const presetArgs = Array.isArray(handler.args) ? handler.args : [];
        return runDirectComponentAction(
          parent.id,
          handler.name,
          presetArgs.concat(forwardedArgs)
        );
      }

      function runDirectComponentAction(componentId, actionName, args) {
        if (!browserLocalRuntime) return false;
        const instance = findRuntimeComponentInstance(componentId);
        if (!instance) return false;
        if (!instance.element) instance.element = componentElementFor(instance.id);
        const definition = componentDefinitionForInstance(instance);
        if (!definition || !Array.isArray(definition.bodyPlan)) return false;
        const actionNode = definition.bodyPlan.find(function (node) {
          return node && node.kind === 'action' && node.name === actionName;
        });
        if (!actionNode) return emitComponentEvent(instance, actionName, args);
        const scope = componentScopeFor(instance, definition);
        const actionWords = splitComponentWords(actionNode.text || '');
        const previous = {};
        const hadPrevious = {};
        (actionWords.slice(2) || []).forEach(function (param, index) {
          if (!param) return;
          hadPrevious[param] = Object.prototype.hasOwnProperty.call(scope, param);
          previous[param] = scope[param];
          scope[param] = (args || [])[index];
        });
        const changes = { writes: {} };
        const actionContext = {
          name: actionNode.name || actionName || '',
          sourceLine: actionNode.sourceLine || 0,
          text: actionNode.text || ''
        };
        const handled = runComponentPlanStatements(
          definition,
          scope,
          componentNodeChildren(definition, actionNode),
          instance,
          changes,
          actionContext);
        (actionWords.slice(2) || []).forEach(function (param) {
          if (!param) return;
          if (hadPrevious[param]) scope[param] = previous[param];
          else delete scope[param];
        });
        if (!handled) {
          return false;
        }
        const shouldRender = componentWritesAffectRender(definition, Object.keys(changes.writes || {}));
        noteComponentActionResult(instance, definition, actionName, changes.writes, !shouldRender);
        if (shouldRender &&
            !patchDirectComponentFromWrites(instance, definition, changes.writes)) {
          renderDirectComponent(instance);
        }
        return true;
      }

      function runDirectComponentFallbackAction(componentId, actionName) {
        if (!browserLocalRuntime) return false;
        const instance = findRuntimeComponentInstance(componentId);
        if (!instance) return false;
        const loweredName = instance.locals && instance.locals[actionName]
          ? instance.locals[actionName]
          : actionName;
        return executeClientAction(loweredName, []);
      }

      async function runLiveComponentAction(componentId, actionName, args) {
        if (browserLocalRuntime || !componentId || !actionName) return false;
        try {
          const res = await fetch('/api/component-action', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
              componentId: componentId,
              action: actionName,
              args: args || []
            })
          });
          const data = await res.json();
          if (!data || data.ok === false) {
            if (data && data.error) reportStatus('error', data.error);
            return false;
          }
          if (data.bindings) applyBindings(data.bindings);
          if (data.renderedComponents) applyLiveBodyPlanRender(data.renderedComponents);
          else if (data.state) applyLiveBodyPlanRender(data.state);
          if (data.components && window.jtml) window.jtml.liveComponents = data.components;
          return true;
        } catch (err) {
          reportStatus('error', err && err.message ? err.message : 'component action failed');
          console.error('[jtml] component action failed:', err);
          return false;
        }
      }

      function startDirectComponentBindings() {
        if (document.documentElement.dataset.jtmlDirectComponentBindings === 'true') return;
        document.documentElement.dataset.jtmlDirectComponentBindings = 'true';
        document.addEventListener('click', async function (event) {
          const button = event.target && event.target.closest &&
            event.target.closest('[data-jtml-direct-component-action]');
          if (!button) return;
          const componentId = button.getAttribute('data-jtml-direct-component-id') || '';
          const actionName = button.getAttribute('data-jtml-direct-component-action') || '';
          let args = [];
          try {
            args = JSON.parse(button.getAttribute('data-jtml-direct-component-args') || '[]');
          } catch (_) {
            args = [];
          }
          const handled = browserLocalRuntime
            ? (runDirectComponentAction(componentId, actionName, args) ||
               runDirectComponentFallbackAction(componentId, actionName))
            : await runLiveComponentAction(componentId, actionName, args);
          if (handled) {
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
              moduleId: record.moduleId == null ? null : record.moduleId,
              definitionModule: record.definitionModule == null ? null : record.definitionModule,
              id: record.id || '',
              component: record.component || '',
              instanceId: Number(record.instanceId || 0),
              role: record.role || 'component',
              params: record.params || {},
              locals: record.locals || {},
              slotPlan: Array.isArray(record.slotPlan) ? record.slotPlan.slice() : [],
              sourceLine: Number(record.sourceLine || 0),
              element: componentElementFor(record.id || '')
            };
          });
        } else {
          componentInstances = Array.prototype.slice.call(
            document.querySelectorAll('[data-jtml-instance]')
          ).map(function (el) {
            return {
              moduleId: null,
              definitionModule: null,
              id: el.getAttribute('data-jtml-instance') || '',
              component: el.getAttribute('data-jtml-component') || '',
              instanceId: Number(el.getAttribute('data-jtml-instance-id') || 0),
              role: el.getAttribute('data-jtml-component-role') || 'component',
              params: parseComponentMap(el.getAttribute('data-jtml-component-params') || ''),
              locals: parseComponentMap(el.getAttribute('data-jtml-component-locals') || ''),
              slotPlan: bodyPlanFromEncodedSource(
                decodeHex(el.getAttribute('data-jtml-component-slot-hex') || '')
              ),
              sourceLine: Number(el.getAttribute('data-jtml-source-line') || 0),
              element: el
            };
          });
        }
        if (manifestDefinitions.length) {
          componentDefinitions = manifestDefinitions.map(function (record) {
            return {
              moduleId: record.moduleId == null ? null : record.moduleId,
              name: record.name || '',
              params: Array.isArray(record.params) ? record.params.slice() : [],
              emits: Array.isArray(record.emits) ? record.emits.slice() : [],
              emitArity: record.emitArity && typeof record.emitArity === 'object' ? Object.assign({}, record.emitArity) : {},
              emitPayloads: record.emitPayloads && typeof record.emitPayloads === 'object' ? Object.assign({}, record.emitPayloads) : {},
              emitPayloadTypes: record.emitPayloadTypes && typeof record.emitPayloadTypes === 'object' ? Object.assign({}, record.emitPayloadTypes) : {},
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
                emits: Array.isArray(record.emits) ? record.emits.slice() : [],
                emitArity: record.emitArity && typeof record.emitArity === 'object' ? Object.assign({}, record.emitArity) : {},
                emitPayloads: record.emitPayloads && typeof record.emitPayloads === 'object' ? Object.assign({}, record.emitPayloads) : {},
                emitPayloadTypes: record.emitPayloadTypes && typeof record.emitPayloadTypes === 'object' ? Object.assign({}, record.emitPayloadTypes) : {},
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
              moduleId: null,
              name: el.getAttribute('data-jtml-component-def') || '',
              params: String(el.getAttribute('data-jtml-component-def-params') || '').split(';').filter(Boolean),
              emits: String(el.getAttribute('data-jtml-component-def-emits') || '').split(';').filter(Boolean),
              emitArity: parseComponentIntMap(el.getAttribute('data-jtml-component-def-emit-arity') || ''),
              emitPayloads: parseComponentStringListMap(el.getAttribute('data-jtml-component-def-emit-payloads') || ''),
              emitPayloadTypes: parseComponentStringListMap(el.getAttribute('data-jtml-component-def-emit-payload-types') || ''),
              sourceLine: Number(el.getAttribute('data-jtml-source-line') || 0),
              body: decodeHex(el.getAttribute('data-jtml-component-body-hex') || ''),
              runtimePlan: {
                mode: 'expanded-compatibility',
                ownsEnvironment: true,
                bodyNodeCount: 0,
                rootTemplateNodeCount: 0,
                slotCount: 0,
                emits: String(el.getAttribute('data-jtml-component-def-emits') || '').split(';').filter(Boolean),
                emitArity: parseComponentIntMap(el.getAttribute('data-jtml-component-def-emit-arity') || ''),
                emitPayloads: parseComponentStringListMap(el.getAttribute('data-jtml-component-def-emit-payloads') || ''),
                emitPayloadTypes: parseComponentStringListMap(el.getAttribute('data-jtml-component-def-emit-payload-types') || ''),
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
          const definition = componentDefinitionForInstance(instance) || {};
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

      function bodyPlanFromEncodedSource(source) {
        const lines = String(source || '').split(/\r?\n/);
        const plan = [];
        const ancestors = [];
        const cleanName = function (token) {
          return String(token || '').replace(/[\\(),:]+$/g, '').split(':')[0];
        };
        const postfixAssignment = function (token) {
          token = String(token || '');
          if (token.length <= 2) return null;
          if (token.slice(-2) === '++') return { name: cleanName(token.slice(0, -2)), operator: '+=', expression: '1' };
          if (token.slice(-2) === '--') return { name: cleanName(token.slice(0, -2)), operator: '-=', expression: '1' };
          return null;
        };
        const kindFor = function (head, words) {
          if (words.length >= 2 && ['=', '+=', '-=', '*=', '/=', '%='].indexOf(words[1]) !== -1) return 'assignment';
          if (words.length === 1 && postfixAssignment(words[0])) return 'assignment';
          if (words.length === 1 && /^[A-Za-z_][A-Za-z0-9_]*\(.*\)$/.test(words[0])) return 'call';
          if (head === 'let' || head === 'const') return 'state';
          if (head === 'get') return 'derived';
          if (head === 'when') return 'action';
          if (head === 'effect') return 'effect';
          if (head === 'slot') return 'slot';
          return 'template';
        };
        lines.forEach(function (line) {
          if (!line) return;
          const colon = line.indexOf(':');
          const indent = colon === -1 ? 0 : Number(line.slice(0, colon) || 0);
          const text = (colon === -1 ? line : line.slice(colon + 1)).trim();
          if (!text) return;
          const words = splitComponentWords(text);
          if (!words.length) return;
          while (ancestors.length && plan[ancestors[ancestors.length - 1]].indent >= indent) {
            ancestors.pop();
          }
          const parentIndex = ancestors.length ? ancestors[ancestors.length - 1] : -1;
          const head = words[0];
          const kind = kindFor(head, words);
          const postfix = kind === 'assignment' && words.length === 1 ? postfixAssignment(words[0]) : null;
          const keyIndex = head === 'for' ? words.indexOf('key') : -1;
          const templateExpression = kind === 'template'
            ? (keyIndex === -1 ? words.slice(1).join(' ') : words.slice(1, keyIndex).join(' '))
            : '';
          const node = {
            indent: indent,
            parentIndex: parentIndex,
            childIndices: [],
            kind: kind,
            head: head,
            name: postfix ? postfix.name : (kind === 'assignment' ? cleanName(words[0]) :
              (kind === 'call' ? cleanName(words[0].slice(0, words[0].indexOf('('))) :
              (['state', 'derived', 'action', 'effect'].indexOf(kind) !== -1 ? cleanName(words[1]) :
                (kind === 'template' ? cleanName(head) : '')))),
            text: text,
            operator: postfix ? postfix.operator : (kind === 'assignment' ? words[1] : ''),
            expression: postfix ? postfix.expression : (kind === 'assignment' ? words.slice(2).join(' ') :
              (kind === 'call'
                ? words[0].slice(words[0].indexOf('(') + 1, -1)
                : templateExpression)),
            keyExpression: keyIndex === -1 ? '' : words.slice(keyIndex + 1).join(' '),
            renderRoot: indent === 0 && kind === 'template'
          };
          if (['state', 'derived', 'action', 'effect'].indexOf(kind) !== -1) {
            const eq = words.indexOf('=');
            if (eq !== -1) {
              node.operator = '=';
              node.expression = words.slice(eq + 1).join(' ');
            }
          }
          plan.push(node);
          if (parentIndex >= 0 && plan[parentIndex]) {
            plan[parentIndex].childIndices.push(plan.length - 1);
          }
          ancestors.push(plan.length - 1);
        });
        return plan;
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

      function parseClientPathSegments(path, scope) {
        const source = normalizeClientExpr(path);
        const parts = [];
        let i = 0;
        function readIdentifier() {
          const start = i;
          while (i < source.length && /[A-Za-z0-9_]/.test(source[i])) i += 1;
          return source.slice(start, i);
        }
        function readBracket() {
          i += 1;
          let current = '';
          let quote = '';
          let depth = 0;
          while (i < source.length) {
            const ch = source[i];
            if (quote) {
              current += ch;
              if (ch === '\\' && i + 1 < source.length) current += source[++i];
              else if (ch === quote) quote = '';
              i += 1;
              continue;
            }
            if (ch === '"' || ch === "'") {
              quote = ch;
              current += ch;
              i += 1;
              continue;
            }
            if (ch === '[' || ch === '(' || ch === '{') depth += 1;
            if ((ch === ')' || ch === '}') && depth > 0) depth -= 1;
            if (ch === ']' && depth === 0) {
              i += 1;
              break;
            }
            if (ch === ']' && depth > 0) depth -= 1;
            current += ch;
            i += 1;
          }
          const raw = current.trim();
          if (!raw) return '';
          if ((raw[0] === '"' && raw[raw.length - 1] === '"') ||
              (raw[0] === "'" && raw[raw.length - 1] === "'")) {
            return raw.slice(1, -1);
          }
          if (/^-?\d+$/.test(raw)) return Number(raw);
          const evaluated = evaluateClientExpression(raw, scope);
          return evaluated.found ? evaluated.value : raw;
        }
        while (i < source.length) {
          if (source[i] === '.') {
            i += 1;
            continue;
          }
          if (source[i] === '[') {
            parts.push(readBracket());
            continue;
          }
          if (/[A-Za-z_]/.test(source[i])) {
            const ident = readIdentifier();
            if (!ident) return [];
            parts.push(ident);
            continue;
          }
          return [];
        }
        return parts;
      }

      function deepGet(scope, path) {
        const parts = parseClientPathSegments(path, scope);
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

      function assignObjectPath(root, parts, value) {
        if (!parts.length) return false;
        let target = root;
        for (let i = 0; i < parts.length - 1; i += 1) {
          const key = parts[i];
          if (target == null || typeof target !== 'object') return false;
          if (target[key] == null) {
            target[key] = typeof parts[i + 1] === 'number' ? [] : {};
          }
          if (typeof target[key] !== 'object') return false;
          target = target[key];
        }
        if (target == null || typeof target !== 'object') return false;
        target[parts[parts.length - 1]] = value;
        return true;
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
        if (/^[A-Za-z_][A-Za-z0-9_]*(?:(?:\.[A-Za-z_][A-Za-z0-9_]*)|(?:\[[^\]]+\]))*$/.test(expr)) {
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
        const parts = parseClientPathSegments(path, clientState);
        if (!parts.length) return;
        if (parts.length === 1) {
          clientState[parts[0]] = value;
          return;
        }
        if (clientState[parts[0]] == null || typeof clientState[parts[0]] !== 'object') {
          clientState[parts[0]] = typeof parts[1] === 'number' ? [] : {};
        }
        assignObjectPath(clientState[parts[0]], parts.slice(1), value);
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
        if (!Array.isArray(bindings)) {
          Object.keys(bindings || {}).forEach(function (name) {
            if (overwrite || !ownProperty(target, name)) target[name] = bindings[name];
          });
          return target;
        }
        bindings.forEach(function (binding) {
          if (!binding || !binding.name) return;
          if (overwrite || !ownProperty(target, binding.name)) {
            target[binding.name] = binding.expr || '';
          }
        });
        return target;
      }

      function mergeRuntimeActions(target, actions, overwrite) {
        if (!Array.isArray(actions)) {
          Object.keys(actions || {}).forEach(function (name) {
            if (overwrite || !ownProperty(target, name)) target[name] = actions[name];
          });
          return target;
        }
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
        const moduleId = options.moduleId == null ? null : options.moduleId;
        const stampModule = function (item) {
          if (item && moduleId != null && item.moduleId == null) item.moduleId = moduleId;
          return item;
        };
        mergeRuntimeBindings(merged.state, plan.state, false);
        mergeRuntimeBindings(merged.derived, plan.derived, false);
        mergeRuntimeActions(merged.actions, plan.actions, false);
        mergeRuntimeArray(merged.routes, plan.routes, function (route) {
          return String(route.path || '') + '|' + String(route.component || '');
        }, false);
        mergeRuntimeArray(merged.fetches, plan.fetches, function (fetchNode) {
          return String(fetchNode.name || '') + '|' + String(fetchNode.url || '');
        }, false);
        mergeRuntimeArray(merged.componentDefinitions, (plan.componentDefinitions || []).map(stampModule), function (definition) {
          return String(definition.moduleId == null ? '' : definition.moduleId) + '|' +
                 String(definition.name || '');
        }, preferDefinitions);
        mergeRuntimeArray(merged.componentInstances, (plan.componentInstances || []).map(stampModule), function (instance) {
          return String(instance.moduleId == null ? '' : instance.moduleId) + '|' +
                 String(instance.id || '') + '|' +
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
            if (!modulePlan || modulePlan.executable === false) return;
            mergeRuntimePlanIntoManifest(
              merged,
              modulePlan && modulePlan.plan,
              { preferDefinitions: true, moduleId: modulePlan && modulePlan.id }
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
          staticComponentPlanIndex: window.__jtml_static_component_plan_index || null,
          staticUpdatePlans: window.__jtml_static_update_plans || null,
          staticUpdateFunctions: window.__jtml_static_update_functions || null,
          staticComponentModules: window.__jtml_static_component_modules || null,
          runtimeSecurity: {
            cspSafeUpdatePlans: !dynamicGeneratedUpdateFunctions,
            dynamicGeneratedUpdateFunctions: dynamicGeneratedUpdateFunctions,
            generatedUpdateSourceAvailable: true,
            staticComponentPlanIndexAsset: !!window.__jtml_static_component_plan_index,
            staticUpdatePlansAsset: !!window.__jtml_static_update_plans,
            staticUpdateFunctionsAsset: !!window.__jtml_static_update_functions,
            staticComponentModulesAsset: !!window.__jtml_static_component_modules,
            staticComponentPlanIndexLoaded: !!window.__jtml_static_component_plan_index,
            staticUpdatePlansLoaded: !!window.__jtml_static_update_plans,
            staticUpdateFunctionsLoaded: !!window.__jtml_static_update_functions,
            staticComponentModulesLoaded: !!window.__jtml_static_component_modules
          },
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

)JTR1";
}

const char* browserRuntimeDataPlatformChunk() {
    return R"JTR2(      function startFetchBindings() {
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

      function jtmlStableHash(value) {
        value = String(value == null ? '' : value);
        let hash = 2166136261;
        const bytes = typeof TextEncoder !== 'undefined'
          ? new TextEncoder().encode(value)
          : null;
        const length = bytes ? bytes.length : value.length;
        for (let i = 0; i < length; i += 1) {
          hash ^= bytes ? bytes[i] : (value.charCodeAt(i) & 255);
          hash = Math.imul(hash, 16777619);
        }
        return (hash >>> 0).toString(16);
      }

      function hasLiveBodyPlanServerRender() {
        return !!document.querySelector('[data-jtml-live-body-plan-transport="body-plan"]');
      }

      function applyLiveBodyPlanRender(payload) {
        if (browserLocalRuntime || !payload) return;
        const records = Array.isArray(payload.components)
          ? payload.components
          : (payload.state && Array.isArray(payload.state.components) ? payload.state.components : []);
        let patched = 0;
        let current = 0;
        let unsupported = 0;
        let missingElement = 0;
        const patchedIds = [];
        const currentIds = [];
        const unsupportedIds = [];
        const missingElementIds = [];
        records.forEach(function (component) {
          if (!component || !component.id) return;
          const runtime = component.runtime || {};
          const supported = component.supported === true ||
            component.renderedHtmlSupported === true ||
            runtime.renderedHtmlSupported === true ||
            runtime.bodyPlanTemplateRendering === true;
          const html = component.renderedHtml;
          if (!supported || typeof html !== 'string' || !html.length) {
            unsupported += 1;
            unsupportedIds.push(component.id || '');
            return;
          }
          const el = componentElementFor(component.id);
          if (!el) {
            missingElement += 1;
            missingElementIds.push(component.id || '');
            return;
          }
          const htmlHash = jtmlStableHash(html);
          if (el.dataset.jtmlLiveBodyPlanRenderedHash === htmlHash) {
            el.dataset.jtmlLiveBodyPlanTransport = 'body-plan';
            el.dataset.jtmlLiveBodyPlanRendered = 'current';
            current += 1;
            currentIds.push(component.id || '');
            return;
          }
          el.innerHTML = html;
          el.dataset.jtmlLiveBodyPlanRenderedHash = htmlHash;
          el.dataset.jtmlLiveBodyPlanRendered = 'current';
          el.dataset.jtmlLiveBodyPlanTransport = 'body-plan';
          patched += 1;
          patchedIds.push(component.id || '');
        });
        if (records.length) {
          window.jtml = Object.assign(window.jtml || {}, {
            liveBodyPlanTransport: true,
            liveBodyPlanPatchCount: patched,
            liveBodyPlanLastPatch: {
              componentCount: records.length,
              patched: patched,
              current: current,
              unsupported: unsupported,
              missingElement: missingElement,
              patchedIds: patchedIds,
              currentIds: currentIds,
              unsupportedIds: unsupportedIds,
              missingElementIds: missingElementIds,
              mode: 'live-body-plan-render'
            }
          });
        }
        if (patched) {
          renderCharts();
          processImageBindings();
          applyRoutes();
        }
      }

      async function refreshLiveBodyPlanRender(options) {
        if (browserLocalRuntime) return;
        if (!(options && options.force === true) && hasLiveBodyPlanServerRender()) return;
        try {
          const res = await fetch('/api/rendered-components');
          const payload = await res.json();
          applyLiveBodyPlanRender(payload);
        } catch (_) {
          // The compatibility DOM remains authoritative if the endpoint is not
          // available, so this transport layer fails closed.
        }
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

)JTR2";
}

const char* browserRuntimeBootTransportChunk() {
    return R"JTR3(      function applyInitial() {
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
        refreshLiveBodyPlanRender();
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
            if (m.type === 'populateBindings' && m.bindings) {
              applyBindings(m.bindings);
              refreshLiveBodyPlanRender();
            }
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
          if (data && data.renderedComponents) applyLiveBodyPlanRender(data.renderedComponents);
          else if (data && data.state) applyLiveBodyPlanRender(data.state);
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
)JTR3";
}

} // namespace

BrowserRuntimeAssetChunks browserRuntimeAssetChunks() {
    return {{
        {"prelude", browserRuntimePreludeChunk()},
        {"components", browserRuntimeComponentChunk()},
        {"data-platform", browserRuntimeDataPlatformChunk()},
        {"boot-transport", browserRuntimeBootTransportChunk()},
    }};
}

} // namespace jtml
