# Event & Response JSON Schemas (schemaVersion 3)

Purpose: Machine-readable contracts for all outbound events and HTTP responses.

Structure:
- base-event.schema.json: Root event fields & shared $defs.
- call-*.schema.json: Call lifecycle events.
- phone-state.schema.json: Phone state variants (event enumerated).
- system-stats.schema.json: Example system event (extend similarly for status/shutdown/error if needed).
- config-delta-single / config-delta-aggregated: Config mutation signaling.
- response-success / response-error: HTTP response contracts.

Conventions:
- draft 2020-12 meta-schema.
- additionalProperties: false for subtype payload sections to prevent drift.
- Aggregated config deltas supply changes[]. Single deltas use key/newValue (optional oldValue).

Future Work:
- Add schemas for remaining system subtypes (status, shutdown, error) if consumers need strict validation.
- Optional generation script to bundle for frontend consumption.
