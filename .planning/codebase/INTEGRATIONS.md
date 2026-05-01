# External Integrations

**Analysis Date:** 2026-05-01

## APIs & External Services

**External APIs:**
- None - Pure standalone cryptocurrency search engine
- No HTTP/REST API dependencies
- No cloud service integrations

## Data Storage

**Databases:**
- None - No database integration
- Target data loaded from flat text files (`puzzles/*.txt`)
- Found results written to `found.txt` (local filesystem)

**File Storage:**
- Local filesystem only
- Target files: `puzzles/` directory (text files with Bitcoin addresses/pubkeys)
- Checkpoint files: user-specified path via CLI
- Trap directories: user-specified path via `--trap-dir`
- Output files: `found.txt` (auto-created in working directory)

**Caching:**
- None - No external caching service
- In-memory Cuckoo filter for target matching
- Checkpoint files for state persistence

## Authentication & Identity

**Auth Provider:**
- Not applicable - Offline cryptography engine
- No authentication required

## Monitoring & Observability

**Error Tracking:**
- None - No external error tracking service
- Errors written to stderr

**Logs:**
- Console output via `std::cout`/`std::cerr`
- No external logging service

## CI/CD & Deployment

**Hosting:**
- Self-hosted (no cloud hosting)
- Build artifacts are standalone executables

**CI Pipeline:**
- None - No automated CI/CD
- Manual build via `make`

## Environment Configuration

**Required env vars:**
- None required - All configuration via CLI

**Secrets location:**
- Not applicable - No secrets stored

## Webhooks & Callbacks

**Incoming:**
- None - No incoming webhook endpoints

**Outgoing:**
- None - No outgoing webhook calls

---

*Integration audit: 2026-05-01*