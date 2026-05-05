// app/api/results/[id]/route.ts

import { blockchainResults } from '@/lib/blockchain'
import { supabaseAdmin } from '@/lib/supabase-admin'
import { NextRequest, NextResponse } from 'next/server'

type RouteContext = {
  params: Promise<{ id: string }>
}

export async function GET(req: NextRequest, { params }: RouteContext) {
  try {
    // ── Auth ───────────────────────────────────────────────────
    const token = req.headers.get('authorization')?.replace('Bearer ', '')

    if (!token) {
      return NextResponse.json({ error: 'Unauthorized' }, { status: 401 })
    }

    const db = supabaseAdmin()

    const {
      data: { user },
      error: authError,
    } = await db.auth.getUser(token)

    if (authError || !user) {
      return NextResponse.json({ error: 'Unauthorized' }, { status: 401 })
    }

    const { id: electionId } = await params

    // ── Load election chain + candidates ──────────────────────
    const results = await Promise.allSettled([
      db
        .from('blockchain_state')
        .select('chain_json')
        .eq('election_id', electionId)
        .maybeSingle(),

      db
        .from('candidates')
        .select('*')
        .eq('election_id', electionId)
        .order('name'),
    ])

    const [chainResult, candidatesResult] = results

    if (chainResult.status !== 'fulfilled' || chainResult.value.error) {
      throw chainResult.status === 'fulfilled'
        ? chainResult.value.error
        : chainResult.reason
    }

    if (
      candidatesResult.status !== 'fulfilled' ||
      candidatesResult.value.error
    ) {
      throw candidatesResult.status === 'fulfilled'
        ? candidatesResult.value.error
        : candidatesResult.reason
    }

    const chainRow = chainResult.value.data
    const candidates = candidatesResult.value.data ?? []

    // ── No votes yet ──────────────────────────────────────────
    if (!chainRow) {
      return NextResponse.json({
        tally: {},
        candidates,
        block_count: 0,
        is_valid: true,
        chain_json: '[]',
        merkle_root: '0'.repeat(64),
      })
    }

    // ── Validate + tally + Merkle root via WASM ───────────────
    const { tally, isValid, blockCount, merkleRoot } =
      await blockchainResults(chainRow.chain_json, electionId)

    return NextResponse.json({
      tally,
      candidates,
      block_count: blockCount,
      is_valid: isValid,
      chain_json: chainRow.chain_json,
      merkle_root: merkleRoot,
    })
  } catch (error) {
    console.error('Results API error:', error)
    return NextResponse.json(
      { error: 'Failed to load results' },
      { status: 500 }
    )
  }
}