#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JTML="${JTML_BIN:-"${ROOT}/build/jtml"}"
FIXTURES="${ROOT}/tests/fixtures/performance"
OUT_ROOT="${ROOT}/dist/runtime-benchmarks"

INDEX_BUDGET_BYTES="${JTML_INDEX_BUDGET_BYTES:-50000}"
RUNTIME_BUDGET_BYTES="${JTML_RUNTIME_BUDGET_BYTES:-260000}"
COMPONENT_BUDGET_BYTES="${JTML_COMPONENT_BUDGET_BYTES:-180000}"
APP_BUDGET_BYTES="${JTML_APP_BUDGET_BYTES:-20000}"
PLAN_BUDGET_BYTES="${JTML_PLAN_BUDGET_BYTES:-180000}"

if [[ ! -x "${JTML}" ]]; then
  echo "error: build/jtml is missing; run cmake --build build --target jtml_cli first" >&2
  exit 1
fi

rm -rf "${OUT_ROOT}"
mkdir -p "${OUT_ROOT}"

echo "JTML runtime benchmark smoke"
echo "index budget: ${INDEX_BUDGET_BYTES} bytes"
echo "runtime budget: ${RUNTIME_BUDGET_BYTES} bytes"
echo "component module budget: ${COMPONENT_BUDGET_BYTES} bytes"
echo "app bootstrap budget: ${APP_BUDGET_BYTES} bytes"
echo "update-plan budget: ${PLAN_BUDGET_BYTES} bytes"

for fixture in "${FIXTURES}"/*.jtml; do
  name="$(basename "${fixture}" .jtml)"
  out="${OUT_ROOT}/${name}"
  "${JTML}" build "${fixture}" --target browser --out "${out}" >/dev/null

  index="${out}/index.html"
  runtime="${out}/jtml-runtime.js"
  component_module="${out}/components/index.js"
  app="${out}/app.js"
  plans="${out}/jtml-update-plans.js"
  if [[ ! -f "${index}" || ! -f "${runtime}" || ! -f "${component_module}" || ! -f "${app}" || ! -f "${plans}" ]]; then
    echo "error: ${name} did not emit split browser runtime/component/app assets and legacy update plans" >&2
    exit 1
  fi

  index_bytes="$(wc -c < "${index}" | tr -d ' ')"
  runtime_bytes="$(wc -c < "${runtime}" | tr -d ' ')"
  component_bytes="$(wc -c < "${component_module}" | tr -d ' ')"
  app_bytes="$(wc -c < "${app}" | tr -d ' ')"
  plan_bytes="$(wc -c < "${plans}" | tr -d ' ')"
  if (( index_bytes > INDEX_BUDGET_BYTES )); then
    echo "error: ${name} index.html ${index_bytes} bytes exceeds budget ${INDEX_BUDGET_BYTES}" >&2
    exit 1
  fi
  if (( plan_bytes > PLAN_BUDGET_BYTES )); then
    echo "error: ${name} jtml-update-plans.js ${plan_bytes} bytes exceeds budget ${PLAN_BUDGET_BYTES}" >&2
    exit 1
  fi
  if (( runtime_bytes > RUNTIME_BUDGET_BYTES )); then
    echo "error: ${name} jtml-runtime.js ${runtime_bytes} bytes exceeds budget ${RUNTIME_BUDGET_BYTES}" >&2
    exit 1
  fi
  if (( component_bytes > COMPONENT_BUDGET_BYTES )); then
    echo "error: ${name} components/index.js ${component_bytes} bytes exceeds budget ${COMPONENT_BUDGET_BYTES}" >&2
    exit 1
  fi
  if (( app_bytes > APP_BUDGET_BYTES )); then
    echo "error: ${name} app.js ${app_bytes} bytes exceeds budget ${APP_BUDGET_BYTES}" >&2
    exit 1
  fi

  if ! grep -q "__jtml_static_component_modules" "${component_module}"; then
    echo "error: ${name} component module missing static component module registry" >&2
    exit 1
  fi
  if ! grep -q "__jtml_static_component_plan_index" "${component_module}"; then
    echo "error: ${name} component module missing production component plan index" >&2
    exit 1
  fi
  if grep -q "__jtml_static_update_plans = plans" "${component_module}"; then
    echo "error: ${name} component module still publishes the legacy update-plan global" >&2
    exit 1
  fi
  if grep -q '"bodyPlan"' "${component_module}"; then
    echo "error: ${name} component module contains source-rich bodyPlan debug payload" >&2
    exit 1
  fi
  if ! grep -q '"assetRole":"component-module"' "${component_module}"; then
    echo "error: ${name} component module missing component-module asset role" >&2
    exit 1
  fi
  if grep -q "new Function" "${component_module}"; then
    echo "error: ${name} component module contains dynamic Function constructor" >&2
    exit 1
  fi
  if [[ "${name}" == "control_flow" ]]; then
    if ! grep -q 'data-jtml-direct-region="if"' "${component_module}"; then
      echo "error: ${name} component module missing direct if-region create path" >&2
      exit 1
    fi
    if ! grep -q 'data-jtml-direct-region="for"' "${component_module}"; then
      echo "error: ${name} component module missing direct for-region create path" >&2
      exit 1
    fi
    if ! grep -q 'data-jtml-direct-list-key' "${component_module}"; then
      echo "error: ${name} component module missing keyed list markers" >&2
      exit 1
    fi
    if grep -q 'patchStaticComponentRegionNode' "${component_module}"; then
      echo "error: ${name} component module still delegates safe control-flow patches to runtime region helper" >&2
      exit 1
    fi
  fi
  if [[ "${name}" == "composition" ]]; then
    if ! grep -q 'renderStaticComponentSlotNode(instance, definition, scope' "${component_module}"; then
      echo "error: ${name} component module missing direct slot create contract" >&2
      exit 1
    fi
    if ! grep -q 'renderStaticComponentNestedNode(instance, definition, scope' "${component_module}"; then
      echo "error: ${name} component module missing direct nested component create contract" >&2
      exit 1
    fi
    if ! grep -q 'patchStaticComponentNestedNode(instance, definition' "${component_module}"; then
      echo "error: ${name} component module missing direct nested component patch contract" >&2
      exit 1
    fi
    if ! grep -q 'recordStaticCreateFallback(instance, definition, plan' "${component_module}"; then
      echo "error: ${name} component module missing source-first static create fallback telemetry" >&2
      exit 1
    fi
    if grep -q 'patchStaticComponentRegionNode(instance, definition' "${component_module}"; then
      echo "error: ${name} component module still uses generic region patch helper for composition paths" >&2
      exit 1
    fi
  fi
  if ! grep -q "__jtml_static_update_plans = plans" "${plans}"; then
    echo "error: ${name} legacy update-plan asset no longer publishes compatibility plan global" >&2
    exit 1
  fi
  if ! grep -q '"assetRole":"legacy-update-plan"' "${plans}"; then
    echo "error: ${name} legacy update-plan asset missing compatibility asset role" >&2
    exit 1
  fi

  echo "OK: ${name} index=${index_bytes}B runtime=${runtime_bytes}B components=${component_bytes}B app=${app_bytes}B updatePlans=${plan_bytes}B"
done

echo "JTML runtime benchmark smoke passed."
