# VoteX

Tamper-proof elections powered by a C++ blockchain compiled to WebAssembly.

---

## Overview

VoteX is a full-stack voting application that records every cast vote as an immutable block on a custom blockchain. The cryptographic core is written in C++ and compiled to WebAssembly via Emscripten, running server-side inside Next.js API routes. The front end is a Next.js 14 application backed by Supabase for authentication and persistent storage.

Each vote produces a new block linked to its predecessor by hash. Because every block encodes the hash of the block before it, retroactively altering any vote would invalidate every subsequent block, making tampering detectable at a glance. The chain is verified on every results request, and a Merkle root computed from all block hashes provides a single 64-character integrity fingerprint for the entire election.

---

## Architecture

```
Browser (Next.js client)
    |
    |  HTTPS
    v
Next.js API routes  (Node.js / Edge runtime)
    |-- /api/vote          POST  — cast a vote
    |-- /api/results/[id]  GET   — fetch tally, chain state, Merkle root
    |-- /api/elections     GET   — list active elections
    |
    |  FFI via Emscripten WASM
    v
blockchain.wasm  (C++ core)
    |-- Block mining (proof-of-work)
    |-- Chain validation
    |-- Vote tallying
    |-- Merkle tree computation
    |-- SHA-256 (picosha2, header-only)
    |
    |  Supabase JS client (service role)
    v
Supabase (Postgres + Auth)
    |-- elections
    |-- candidates
    |-- user_votes        (double-vote guard, RLS enforced)
    |-- blockchain_state  (serialised chain JSON per election)
```

---

## Blockchain Core (C++)

The blockchain is implemented in `blockchain.cpp` / `blockchain.hpp` and compiled to a single WASM binary with `build.sh`.

### Data model

Every block stores:

| Field | Type | Description |
|---|---|---|
| `index` | int | Sequential block number |
| `timestamp` | time_t | Unix timestamp at mining time |
| `prev_hash` | string | SHA-256 hash of the preceding block |
| `hash` | string | SHA-256 hash of this block's content |
| `nonce` | int | Proof-of-work counter |
| `election_id` | string | Which election this vote belongs to |
| `voter_hash` | string | SHA-256(user_id) — pseudonymous identifier |
| `candidate_id` | string | The chosen candidate |

The genesis block (index 0) is created automatically with all fields set to sentinel values and a `prev_hash` of 64 zero characters.

### Proof-of-work

Mining increments `nonce` until `SHA-256(index + timestamp + votePayload + prev_hash + nonce)` starts with `difficulty` leading zero characters. The default difficulty is 2. This is intentionally lightweight — the goal is immutability of structure, not energy expenditure.

### Merkle tree

The Merkle root is computed identically to Bitcoin's transaction tree:

1. Seed the leaf level with every block's stored hash (genesis included).
2. Pair adjacent leaves and hash each pair: `SHA-256(left || right)`.
3. If a level has an odd number of nodes, duplicate the last one before pairing.
4. Repeat until a single hash remains.

Any modification to any block changes its hash and therefore every ancestor hash up to the root. The 64-character hex root returned in the results API is a compact integrity fingerprint of the entire chain at the time of the request.

### Chain validation

`isValid()` iterates from block 1 onward and checks three invariants for each block:

- `computeHash()` matches the stored `hash` field (content integrity).
- `prev_hash` matches the preceding block's `hash` (linkage integrity).
- The stored `hash` begins with `difficulty` zero characters (proof-of-work satisfied).

### C bindings (Emscripten)

`bindings.cpp` exposes a flat `extern "C"` API to JavaScript. All functions that return strings allocate via `strdup()` and require the caller to pass the pointer back to `free_string()` when done. The global chain instance is held in a `std::unique_ptr<Blockchain>` and is loaded from serialised JSON at the start of every API request.

Exported functions:

| Function | Description |
|---|---|
| `blockchain_init(difficulty)` | Create a fresh chain |
| `blockchain_load(json)` | Deserialise a chain from JSON |
| `blockchain_add_vote(election_id, voter_hash, candidate_id)` | Mine and append a vote block |
| `blockchain_is_valid()` | Validate the full chain |
| `blockchain_serialize()` | Serialise chain to JSON |
| `blockchain_tally(election_id)` | Count votes per candidate |
| `blockchain_size()` | Number of blocks including genesis |
| `blockchain_merkle_root()` | Compute the Merkle root |
| `sha256(input)` | Hash a string |
| `free_string(ptr)` | Release a string returned by any of the above |

---

## Project Structure

```
.
├── cpp/
│   ├── blockchain.hpp       -- Block and Blockchain class declarations
│   ├── blockchain.cpp       -- Block mining, validation, tally, Merkle tree
│   ├── bindings.cpp         -- Emscripten C bindings
│   ├── picosha2.h           -- Header-only SHA-256 (MIT)
│   ├── test.cpp             -- Native unit test suite
│   └── build.sh             -- Build script (native tests + WASM)
│
├── lib/
│   ├── blockchain.ts        -- Server-side WASM wrapper (Next.js API only)
│   ├── supabase.ts          -- Browser Supabase client (anon key)
│   ├── supabase-admin.ts    -- Server Supabase client (service role)
│   └── wasm/
│       ├── blockchain.js    -- Emscripten JS glue (generated)
│       └── blockchain.wasm  -- Compiled WebAssembly binary (generated)
│
├── app/
│   ├── layout.tsx
│   ├── page.tsx             -- Login / sign-up
│   ├── dashboard/page.tsx   -- Active elections list
│   ├── vote/[electionId]/page.tsx    -- Vote casting
│   ├── results/[electionId]/page.tsx -- Live results + Merkle panel
│   └── api/
│       ├── elections/route.ts
│       ├── results/[id]/route.ts
│       └── vote/route.ts
│
└── types/
    └── index.ts             -- Shared TypeScript types and Supabase schema
```

---

## Database Schema

Four tables are required in Supabase:

```sql
create table elections (
  id          uuid primary key default gen_random_uuid(),
  title       text not null,
  description text,
  start_at    timestamptz not null,
  end_at      timestamptz not null,
  is_active   boolean not null default true,
  created_at  timestamptz not null default now()
);

create table candidates (
  id          uuid primary key default gen_random_uuid(),
  election_id uuid not null references elections(id) on delete cascade,
  name        text not null,
  description text
);

create table user_votes (
  id          uuid primary key default gen_random_uuid(),
  user_id     uuid not null references auth.users(id),
  election_id uuid not null references elections(id),
  candidate_id uuid not null references candidates(id),
  voted_at    timestamptz not null default now(),
  unique (user_id, election_id)
);

create table blockchain_state (
  id           uuid primary key default gen_random_uuid(),
  election_id  uuid not null unique references elections(id),
  chain_json   text not null,
  block_count  int not null default 1,
  is_valid     boolean not null default true,
  last_updated timestamptz not null default now()
);
```

Row-level security should be enabled on all tables. The browser client uses the anon key and is subject to RLS policies. The server-side API routes use the service-role key and bypass RLS.

---

## Building

### Prerequisites

- GCC or Clang with C++17 support (for native tests)
- Emscripten SDK (for WASM build)
- Node.js 18+
- A Supabase project

### Native tests

```bash
cd cpp
./build.sh
```

This compiles `blockchain.cpp` and `test.cpp` with g++, produces a `blockchain_test` binary, and runs all unit tests. The test suite covers SHA-256 correctness, genesis block structure, vote adding, chain validation, serialisation round-trips, vote tallying, Merkle tree computation, and error handling.

### WebAssembly

```bash
cd cpp
./build.sh wasm
```

Requires `emcc` on the PATH. Emscripten must be activated (`source emsdk_env.sh`) before running. The output files `blockchain.js` and `blockchain.wasm` are written to `web/lib/wasm/` relative to the cpp directory. Copy or symlink them to `lib/wasm/` in the Next.js project root.

### Clean

```bash
cd cpp
./build.sh clean
```

### Next.js application

```bash
npm install
```

Create a `.env.local` file:

```
NEXT_PUBLIC_SUPABASE_URL=https://your-project.supabase.co
NEXT_PUBLIC_SUPABASE_ANON_KEY=your-anon-key
SUPABASE_SERVICE_ROLE_KEY=your-service-role-key
```

```bash
npm run dev     # development server on http://localhost:3000
npm run build   # production build
npm run start   # production server
```

---

## Vote Flow

1. The user authenticates via Supabase Auth (email/password).
2. On the vote page, the server checks `user_votes` for a prior submission. If one exists the user is redirected to results immediately.
3. On submission, the API route:
   - Verifies the JWT with the service-role client.
   - Confirms the election is active and the candidate belongs to it.
   - Loads the existing serialised chain from `blockchain_state`, or initialises a new one if no votes have been cast yet.
   - Hashes the Supabase user ID with SHA-256 (via WASM) to produce a pseudonymous `voter_hash`.
   - Calls `blockchain_add_vote` in WASM, which mines a new block and appends it.
   - Persists the updated `chain_json` and inserts a row into `user_votes` in a parallel `Promise.all`. Both writes must succeed; if either fails a 500 is returned.
4. The results page fetches `chain_json` from `blockchain_state`, passes it to the WASM module, and returns the tally, validity flag, block count, and Merkle root.

---

## Security Considerations

**Voter privacy.** User IDs are never stored on-chain directly. Only `SHA-256(user_id)` is embedded in each block. An observer with access to the chain JSON cannot reverse the hash to identify a voter without also knowing the original user ID.

**Double-vote prevention.** The `user_votes` table has a unique constraint on `(user_id, election_id)`. The API also performs an explicit pre-check before any blockchain work begins.

**Chain integrity.** Because `prev_hash` links every block to its predecessor, and because the stored `hash` is verified on every `isValid()` call by recomputing it from first principles, any offline modification to `chain_json` in the database is detectable the next time results are requested. The Merkle root provides an additional compact summary that can be published alongside an election for independent verification.

**Service-role key.** `supabase-admin.ts` must never be imported in client components or pages. It is restricted to `app/api/**` route handlers which run only on the server.

**Concurrency.** The current implementation does not use database-level locking when reading and writing `blockchain_state`. Under concurrent vote submissions for the same election, a race condition could cause one write to overwrite another's chain update. For production use, this should be addressed with a Postgres advisory lock or a serialisable transaction wrapping the read-mine-write cycle.

---

## Dependencies

| Dependency | Purpose |
|---|---|
| picosha2 (header-only) | SHA-256 implementation used in the C++ core |
| Emscripten | C++ to WebAssembly compilation |
| Next.js 14 | React framework and API routes |
| Supabase | Authentication and Postgres database |
| Tailwind CSS | Utility-first styling |
| GSAP | UI animations (card stagger, vote bar transitions) |
| react-icons | Icon set (Remix Icons) |

---