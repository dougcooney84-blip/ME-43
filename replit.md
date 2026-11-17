# ME - 43

An original house and techno synthesizer inspired by Serum-style sound-design flexibility, developed incrementally as a JUCE/C++ instrument plugin. Stage A is the MIDI sine-voice foundation for macOS Ventura and Ableton Live.

## Run & Operate

- `pnpm --filter @workspace/api-server run dev` — run the API server (port 5000)
- `pnpm run typecheck` — full typecheck across all packages
- `pnpm run build` — typecheck + build all packages
- `pnpm --filter @workspace/api-spec run codegen` — regenerate API hooks and Zod schemas from the OpenAPI spec
- `pnpm --filter @workspace/db run push` — push DB schema changes (dev only)
- Required env: `DATABASE_URL` — Postgres connection string
- Build the plugin on the target Mac: see `plugins/stage-a-synth/README.md`

## Stack

- pnpm workspaces, Node.js 24, TypeScript 5.9
- API: Express 5
- DB: PostgreSQL + Drizzle ORM
- Validation: Zod (`zod/v4`), `drizzle-zod`
- API codegen: Orval (from OpenAPI spec)
- Build: esbuild (CJS bundle)

## Where things live

- `plugins/stage-a-synth/` — CMake/JUCE Stage A VST3 and Standalone targets
- `artifacts/api-server/` — shared API service scaffold
- `artifacts/mockup-sandbox/` — reusable design preview scaffold

## Architecture decisions

- Keep DSP, plugin state/parameters, and editor code in separate source files.
- Pin JUCE by release tag; use CMake and C++17 for the plugin.
- Stage A targets macOS Ventura and Ableton Live, with VST3 as the host format.

## Product

_Describe the high-level user-facing capabilities of this app once they exist._

## User preferences

_Populate as you build — explicit user instructions worth remembering across sessions._

## Gotchas

_Populate as you build — sharp edges, "always run X before Y" rules._

## Pointers

- See the `pnpm-workspace` skill for workspace structure, TypeScript setup, and package details
