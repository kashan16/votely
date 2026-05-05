// app/api/vote/route.ts

import { blockchainAddVote, blockchainInit, sha256 } from '@/lib/blockchain'
import { supabaseAdmin } from '@/lib/supabase-admin'
import { NextRequest, NextResponse } from 'next/server'

export async function POST(req: NextRequest) {
  // ── 1. Auth ────────────────────────────────────────────────
  const token = req.headers.get('authorization')?.replace('Bearer ', '')
  if (!token) return NextResponse.json({ error: 'Unauthorized' }, { status: 401 })

  const db = supabaseAdmin()
  const { data: { user }, error: authErr } = await db.auth.getUser(token)
  if (authErr || !user) return NextResponse.json({ error: 'Unauthorized' }, { status: 401 })

  // ── 2. Parse body ──────────────────────────────────────────
  const { election_id, candidate_id } = await req.json()
  if (!election_id || !candidate_id)
    return NextResponse.json({ error: 'election_id and candidate_id required' }, { status: 400 })

  // ── 3. Verify election + candidate exist ───────────────────
  const [{ data: election }, { data: candidate }] = await Promise.all([
    db.from('elections').select('id, is_active, end_at').eq('id', election_id).single(),
    db.from('candidates').select('id').eq('id', candidate_id).eq('election_id', election_id).single(),
  ])

  if (!election || !election.is_active || new Date(election.end_at) < new Date())
    return NextResponse.json({ error: 'Election not active' }, { status: 400 })
  if (!candidate)
    return NextResponse.json({ error: 'Invalid candidate' }, { status: 400 })

  // ── 4. Double-vote check ───────────────────────────────────
  const { data: existing } = await db
    .from('user_votes')
    .select('id')
    .eq('user_id', user.id)
    .eq('election_id', election_id)
    .maybeSingle()

  if (existing) return NextResponse.json({ error: 'Already voted' }, { status: 409 })

  // ── 5. Load (or init) blockchain for this election ─────────
  const { data: chainRow } = await db
    .from('blockchain_state')
    .select('chain_json')
    .eq('election_id', election_id)
    .maybeSingle()

  const currentChain = chainRow?.chain_json ?? await blockchainInit(2)

  // ── 6. Add vote block ──────────────────────────────────────
  const voterHash = await sha256(user.id)
  const { chainJson: updatedChain } = await blockchainAddVote(
    currentChain, election_id, voterHash, candidate_id,
  )

  // ── 7. Persist both writes (best-effort transaction) ───────
  const blockCount = JSON.parse(updatedChain).length

  const [chainResult, voteResult] = await Promise.all([
    db.from('blockchain_state').upsert(
      { election_id, chain_json: updatedChain, block_count: blockCount, is_valid: true },
      { onConflict: 'election_id' },
    ),
    db.from('user_votes').insert({ user_id: user.id, election_id, candidate_id }),
  ])

  if (chainResult.error || voteResult.error) {
    console.error('Write error:', chainResult.error ?? voteResult.error)
    return NextResponse.json({ error: 'Failed to record vote' }, { status: 500 })
  }

  return NextResponse.json({ ok: true, block_count: blockCount })
}