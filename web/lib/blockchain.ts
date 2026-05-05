// lib/blockchain.ts
// Server-side only — loads the C++ WASM module and wraps every exported function.
// Only import this inside app/api/** route handlers.

/* eslint-disable @typescript-eslint/no-explicit-any */
import path from 'path'

let _mod: any = null

async function getModule() {
  if (_mod) return _mod

  // eslint-disable-next-line @typescript-eslint/no-require-imports
  const factory = require('./wasm/blockchain.js')

  _mod = await factory({
    locateFile: (file: string) =>
      path.resolve(process.cwd(), 'lib/wasm', file),
  })

  return _mod
}

// ── helpers for C string in/out ──────────────────────────────

function readStr(mod: any, ptr: number): string {
  const s = mod.UTF8ToString(ptr)
  mod._free_string(ptr)
  return s
}

function writeStr(mod: any, s: string): number {
  const len = mod.lengthBytesUTF8(s) + 1
  const ptr = mod._malloc(len)
  mod.stringToUTF8(s, ptr, len)
  return ptr
}

// ── Public API ───────────────────────────────────────────────

/** Initialise a fresh blockchain. Call before the first addVote on a new election. */
export async function blockchainInit(difficulty = 2): Promise<string> {
  const mod = await getModule()
  mod._blockchain_init(difficulty)
  const ptr = mod._blockchain_serialize()
  return readStr(mod, ptr)
}

/** Load an existing chain from its stored JSON, add a vote, return the updated chain JSON. */
export async function blockchainAddVote(
  chainJson: string,
  electionId: string,
  voterHash: string,
  candidateId: string,
): Promise<{ chainJson: string; blockHash: string }> {
  const mod = await getModule()

  const jsonPtr = writeStr(mod, chainJson)
  const loaded = mod._blockchain_load(jsonPtr)
  mod._free(jsonPtr)
  if (!loaded) throw new Error('Failed to deserialize chain')

  const ePtr = writeStr(mod, electionId)
  const vPtr = writeStr(mod, voterHash)
  const cPtr = writeStr(mod, candidateId)
  const hashPtr = mod._blockchain_add_vote(ePtr, vPtr, cPtr)
  mod._free(ePtr); mod._free(vPtr); mod._free(cPtr)

  if (!hashPtr) throw new Error('Failed to add vote block')
  const blockHash = readStr(mod, hashPtr)

  const chainPtr = mod._blockchain_serialize()
  const updatedChain = readStr(mod, chainPtr)

  return { chainJson: updatedChain, blockHash }
}

/**
 * Validate a chain, tally votes, and compute the Merkle root.
 *
 * The Merkle root is a single SHA-256 hash derived from all block hashes
 * in the chain, providing a compact integrity fingerprint. Any tampered
 * block will produce a completely different root.
 */
export async function blockchainResults(
  chainJson: string,
  electionId: string,
): Promise<{
  tally: Record<string, number>
  isValid: boolean
  blockCount: number
  merkleRoot: string
}> {
  const mod = await getModule()

  const jsonPtr = writeStr(mod, chainJson)
  const loaded = mod._blockchain_load(jsonPtr)
  mod._free(jsonPtr)
  if (!loaded) throw new Error('Failed to deserialize chain')

  const isValid = mod._blockchain_is_valid() === 1
  const blockCount: number = mod._blockchain_size()

  const ePtr = writeStr(mod, electionId)
  const tallyPtr = mod._blockchain_tally(ePtr)
  mod._free(ePtr)
  const tallyJson = readStr(mod, tallyPtr)
  const tally: Record<string, number> = JSON.parse(tallyJson)

  // Compute Merkle root — the C++ side does the full tree reduction
  const merklePtr = mod._blockchain_merkle_root()
  const merkleRoot = merklePtr ? readStr(mod, merklePtr) : '0'.repeat(64)

  return { tally, isValid, blockCount, merkleRoot }
}

/** SHA-256 a string (used to hash user IDs before embedding in blocks). */
export async function sha256(input: string): Promise<string> {
  const mod = await getModule()
  const ptr = writeStr(mod, input)
  const hashPtr = mod._sha256(ptr)
  mod._free(ptr)
  return readStr(mod, hashPtr)
}