# Per-file CLI measurements

Fresh compiler: `jtml 0.3.0-dev`. Base main `2cec9263d4a2e5fa859c3703615aeb6f2ed5b668`.

Exit codes below are actual process results; `0` does not imply browser acceptance.

| Source | check | lint | clean build | Diagnostic / boundary |
| --- | ---: | ---: | ---: | --- |
| `apps/releasedesk/index.jtml` | 0 | 0 | 0 |  |
| `demos/index.jtml` | 0 | 0 | 1 | 13 issue(s), 0 error(s); build: error: Cannot build clean browser production module plan for demos/index.jtml: Cannot determine module ownership in demos/index.jtml |
| `demos/example.jtml` | 0 | 0 | 0 | 10 issue(s), 0 error(s) |
| `tests/fixtures/production/taskboard-core/index.jtml` | 0 | 0 | 0 | 1 issue(s), 0 error(s) |
| `demos/opspulse/components/layouts/shell.jtml` | 0 | 0 | 0 | 17 issue(s), 0 error(s) |
| `demos/opspulse/components/ui/app-header.jtml` | 0 | 0 | 0 | 17 issue(s), 0 error(s) |
| `demos/opspulse/components/ui/app-sidebar.jtml` | 0 | 0 | 0 | 16 issue(s), 0 error(s) |
| `demos/opspulse/components/ui/common.jtml` | 0 | 0 | 0 | 7 issue(s), 0 error(s) |
| `demos/opspulse/components/ui/filter-panel.jtml` | 0 | 0 | 0 | 9 issue(s), 0 error(s) |
| `demos/opspulse/index.jtml` | 0 | 0 | 0 | 4 issue(s), 0 error(s) |
| `demos/opspulse/pages/dashboard.jtml` | 0 | 0 | 0 | 17 issue(s), 0 error(s) |
| `demos/opspulse/pages/incidents.jtml` | 1 | 1 | 1 | check: demos/opspulse/pages/incidents.jtml: error[JTML_COMPONENT]: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/incidents.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard; lint: error: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/incidents.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard; build: error: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/incidents.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard |
| `demos/opspulse/pages/media.jtml` | 1 | 1 | 1 | check: demos/opspulse/pages/media.jtml: error[JTML_COMPONENT]: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/media.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard; lint: error: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/media.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard; build: error: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/media.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard |
| `demos/opspulse/pages/not-found.jtml` | 1 | 1 | 1 | check: demos/opspulse/pages/not-found.jtml: error[JTML_COMPONENT]: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/not-found.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard; lint: error: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/not-found.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard; build: error: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/not-found.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard |
| `demos/opspulse/pages/people.jtml` | 1 | 1 | 1 | check: demos/opspulse/pages/people.jtml: error[JTML_COMPONENT]: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/people.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard; lint: error: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/people.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard; build: error: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/people.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard |
| `demos/opspulse/pages/releases.jtml` | 1 | 1 | 1 | check: demos/opspulse/pages/releases.jtml: error[JTML_COMPONENT]: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/releases.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero, ProgressBar. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard; lint: error: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/releases.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero, ProgressBar. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard; build: error: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/releases.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero, ProgressBar. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard |
| `demos/opspulse/pages/settings.jtml` | 1 | 1 | 1 | check: demos/opspulse/pages/settings.jtml: error[JTML_COMPONENT]: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/settings.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard; lint: error: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/settings.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard; build: error: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/settings.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard |
| `demos/opspulse/pages/usage.jtml` | 1 | 1 | 1 | check: demos/opspulse/pages/usage.jtml: error[JTML_COMPONENT]: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/usage.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero, ProgressBar. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard; lint: error: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/usage.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero, ProgressBar. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard; build: error: Cannot resolve import '../components/ui/common.jtml' from demos/opspulse/pages/usage.jtml (attempted demos/opspulse/components/ui/common.jtml): Missing export(s) from demos/opspulse/components/ui/common.jtml: PageHero, ProgressBar. Available exports: AlertBar, EmptyBlock, ErrorBlock, LoadingBlock, MetricCard |
| `demos/opspulse/stores/app-state.jtml` | 0 | 0 | 0 | 7 issue(s), 0 error(s) |
| `demos/opspulse/stores/filter-state.jtml` | 0 | 0 | 0 | 9 issue(s), 0 error(s) |
| `demos/opspulse/stores/ops-data.jtml` | 0 | 0 | 0 | 1 issue(s), 0 error(s) |
| `examples/chat_app.jtml` | 0 | 0 | 0 | 1 issue(s), 0 error(s) |
| `examples/dashboard.jtml` | 0 | 0 | 0 |  |
| `examples/dicts.jtml` | 0 | 0 | 0 |  |
| `examples/drag_and_drop_tasks.jtml` | 0 | 0 | 0 |  |
| `examples/dynamic_replacement.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_advanced_form.jtml` | 0 | 0 | 0 | 1 issue(s), 0 error(s) |
| `examples/friendly_component_isolation.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_components.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_counter.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_effect.jtml` | 0 | 0 | 1 | build: error: Cannot build clean browser production module plan for examples/friendly_effect.jtml: Cannot determine module ownership in examples/friendly_effect.jtml |
| `examples/friendly_elements.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_extern.jtml` | 0 | 0 | 0 | 1 issue(s), 0 error(s) |
| `examples/friendly_fetch.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_fetch_invalidate.jtml` | 0 | 0 | 1 | build: error: Cannot build clean browser production module plan for examples/friendly_fetch_invalidate.jtml: Cannot determine module ownership in examples/friendly_fetch_invalidate.jtml |
| `examples/friendly_fetch_post.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_fetch_refresh.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_form.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_import_page.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_interop.jtml` | 0 | 0 | 0 | 3 issue(s), 0 error(s) |
| `examples/friendly_media.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_module_component.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_redirect.jtml` | 0 | 0 | 1 | build: error: Cannot build clean browser production module plan for examples/friendly_redirect.jtml: Cannot determine module ownership in examples/friendly_redirect.jtml |
| `examples/friendly_route_guards.jtml` | 0 | 0 | 1 | 1 issue(s), 0 error(s); build: error: Cannot build clean browser production module plan for examples/friendly_route_guards.jtml: Cannot determine module ownership in examples/friendly_route_guards.jtml |
| `examples/friendly_route_layout.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_route_load.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_routes.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_semantic_ui.jtml` | 0 | 0 | 0 | 1 issue(s), 0 error(s) |
| `examples/friendly_store.jtml` | 0 | 0 | 0 |  |
| `examples/friendly_types.jtml` | 0 | 0 | 0 |  |
| `examples/html_syntax.jtml` | 0 | 0 | 0 |  |
| `examples/imported_keywords.jtml` | 0 | 0 | 0 | 1 issue(s), 0 error(s) |
| `examples/index.jtml` | 0 | 0 | 0 |  |
| `examples/jtml_studio.jtml` | 0 | 0 | 0 | 1 issue(s), 0 error(s) |
| `examples/keywords.jtml` | 0 | 1 | 0 | lint: examples/keywords.jtml: error[JTML_UNDEFINED_NAME]:29:16: undefined variable 'title' |
| `examples/live_input.jtml` | 0 | 0 | 0 |  |
| `examples/login_form.jtml` | 0 | 0 | 0 |  |
| `examples/modules/card.jtml` | 0 | 0 | 0 |  |
| `examples/modules/friendly_message.jtml` | 0 | 0 | 0 | 1 issue(s), 0 error(s) |
| `examples/profile_page.jtml` | 0 | 0 | 0 |  |
| `examples/starter_counter.jtml` | 0 | 0 | 0 |  |
| `examples/studio_logic.jtml` | 0 | 0 | 0 | 13 issue(s), 0 error(s) |
| `examples/styled_profile_page.jtml` | 0 | 0 | 0 |  |
| `examples/user_interactions.jtml` | 0 | 0 | 0 |  |
| `examples/jtl_core.jtl` | 0 | 0 | 0 | 1 issue(s), 0 error(s) |
| `studio/samples/animation.jtml` | 0 | 0 | 0 | 2 issue(s), 0 error(s) |
| `studio/samples/charts.jtml` | 0 | 0 | 0 | 1 issue(s), 0 error(s) |
| `studio/samples/components.jtml` | 0 | 0 | 0 |  |
| `studio/samples/counter.jtml` | 0 | 0 | 0 |  |
| `studio/samples/dashboard.jtml` | 0 | 0 | 0 |  |
| `studio/samples/effects.jtml` | 0 | 0 | 1 | build: error: Cannot build clean browser production module plan for studio/samples/effects.jtml: Cannot determine module ownership in studio/samples/effects.jtml |
| `studio/samples/fetch-post.jtml` | 0 | 0 | 0 |  |
| `studio/samples/fetch.jtml` | 0 | 0 | 1 | 1 issue(s), 0 error(s); build: error: Cannot build clean browser production module plan for studio/samples/fetch.jtml: Cannot determine module ownership in studio/samples/fetch.jtml |
| `studio/samples/form.jtml` | 0 | 0 | 0 |  |
| `studio/samples/media.jtml` | 0 | 0 | 0 | 2 issue(s), 0 error(s) |
| `studio/samples/redirect.jtml` | 0 | 0 | 1 | 1 issue(s), 0 error(s); build: error: Cannot build clean browser production module plan for studio/samples/redirect.jtml: Cannot determine module ownership in studio/samples/redirect.jtml |
| `studio/samples/routes.jtml` | 0 | 0 | 1 | 2 issue(s), 0 error(s); build: error: Cannot build clean browser production module plan for studio/samples/routes.jtml: Cannot determine module ownership in studio/samples/routes.jtml |
| `studio/samples/store.jtml` | 0 | 0 | 0 | 2 issue(s), 0 error(s) |
