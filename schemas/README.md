# Event & Response JSON Schemas (schemaVersion 3)

Machine-readable contracts for every outbound event and HTTP response the firmware produces.

## Directory layout

| File                                                                      | Purpose                                                                                          |
| ------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| `base-event.schema.json`                                                  | Shared `$defs` and required root fields (`category`, `event`, `schemaVersion`).                  |
| `call-*.schema.json`                                                      | Call lifecycle notifications (`start`, `end`, `missed`, etc.).                                   |
| `phone-state.schema.json`                                                 | Phone state snapshots and incremental updates (dialing, call info, DND, ringing flags).          |
| `system-*.schema.json`                                                    | System health/status events (stats, shutdown, error). Add new variants alongside existing files. |
| `config-delta-single.schema.json` / `config-delta-aggregated.schema.json` | Change notifications for configuration mutations.                                                |
| `response-success.schema.json` / `response-error.schema.json`             | Contract for HTTP responses from the HA web server.                                              |

## Authoring checklist

1. **Update schema version when necessary**: bump `INTEGRATION_EVENT_SCHEMA_VERSION` in the firmware when you introduce breaking payload changes, and mirror the new version in the schema `$id`/`schemaVersion` fields.
2. **Name the file after the payload**: prefer `category-event.schema.json` so consumers can auto-map files.
3. **Reference `base-event`**: every event schema should `allOf` the base schema to inherit invariants.
4. **Lock down the shape**: set `additionalProperties: false` for nested objects so extra keys fail validation.
5. **Document new properties**: add short descriptions in the schema to help downstream codegen/documentation.

## Validating changes

We keep validation simple to avoid bespoke tooling:

```powershell
# From the repo root
pip install --user jsonschema
python -m jsonschema --instance samples/phone-state.json schemas/phone-state.schema.json
```

- Store illustrative payloads under `schemas/samples/` (add the folder if it doesn’t exist).
- Validate every new schema against at least one real payload sample before merging.
- If you change a schema, re-run validations for any downstream schemas that `$ref` it.

## Adding new events/responses

1. Copy the closest existing schema into a new file and adjust `$id`, `title`, and enumerations.
2. Add real-world JSON examples in `schemas/samples/` and include them in code review notes.
3. Update the integration service/firmware to emit the new fields and bump schema version if breaking.
4. Notify frontend/integration consumers so they can pull the updated contract.

## Future work

- Round out `system-*.schema.json` with `status`, `shutdown`, and `error` variants once consumers need strict typing.
- Consider a lightweight bundler (e.g., `python -m zipfile`) so the frontend can fetch all schemas in one artifact.
- Evaluate generating TypeScript interfaces from the schemas for the HA frontend once the contracts stabilize.
