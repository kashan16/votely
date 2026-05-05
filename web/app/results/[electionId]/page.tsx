/* eslint-disable react-hooks/set-state-in-effect */
'use client'

import { Badge } from '@/components/ui/badge'
import { supabase } from '@/lib/supabase'
import { Candidate, ResultsResponse } from '@/types'

import { gsap } from 'gsap'
import { useParams, useRouter } from 'next/navigation'
import {
  useCallback,
  useEffect,
  useRef,
  useState,
  useTransition,
} from 'react'
import {
  RiArrowLeftLine,
  RiCheckboxCircleLine,
  RiDatabase2Line,
  RiGitBranchLine,
  RiShieldCheckLine,
  RiShieldLine,
} from 'react-icons/ri'

// ── Tiny Merkle tree visualiser ──────────────────────────────
// Renders at most 3 levels of the tree inline.
// All real computation happens in C++/WASM on the server;
// this component only draws what the API returns.
function MerklePanel({
  merkleRoot,
  blockCount,
  chainJson,
}: {
  merkleRoot: string
  blockCount: number
  chainJson: string
}) {
  // Pull up to 4 leaf hashes from the chain JSON (client-side parse of
  // data we already received — no extra fetch needed).
  const leaves: string[] = []
  try {
    const blocks: { hash: string }[] = JSON.parse(chainJson)
    blocks.forEach(b => leaves.push(b.hash))
  } catch {
    // ignore parse errors — we still show the root
  }

  // Build a visual representation of the bottom two levels only
  // (leaves → level-1 pairs → root).  We cap at 4 leaves for layout.
  const visLeaves = leaves.slice(0, 4)
  const showEllipsis = leaves.length > 4

  // Pair the visible leaves into level-1 nodes
  const lvl1: string[] = []
  for (let i = 0; i < visLeaves.length; i += 2) {
    const l = visLeaves[i]
    const r = visLeaves[i + 1] ?? l          // duplicate if odd
    lvl1.push(`${l.slice(0, 6)}…${r.slice(0, 6)}`)
  }

  const short = (h: string) => `${h.slice(0, 6)}…${h.slice(-4)}`

  return (
    <div className="bg-slate-900 border border-slate-800 rounded-xl p-5 mt-6">
      {/* Header */}
      <div className="flex items-center gap-2 mb-4">
        <RiGitBranchLine className="text-indigo-400 text-lg" />
        <span className="text-slate-300 text-sm font-semibold">
          Merkle Root Verification
        </span>
        <Badge className="bg-indigo-500/10 text-indigo-400 border-indigo-500/20 text-xs ml-auto">
          {blockCount} block{blockCount !== 1 ? 's' : ''}
        </Badge>
      </div>

      {/* Explanation */}
      <p className="text-slate-500 text-xs leading-relaxed mb-5">
        Every block hash is fed into a binary hash tree. Any tampering —
        even a single character — produces a completely different root.
      </p>

      {/* Tree diagram */}
      <div className="space-y-3">
        {/* Root */}
        <div className="flex flex-col items-center gap-1">
          <span className="text-slate-500 text-[10px] uppercase tracking-widest">
            Root
          </span>
          <div className="bg-indigo-500/10 border border-indigo-500/30 rounded-lg px-3 py-1.5 w-full text-center">
            <span className="font-mono text-xs text-indigo-300 break-all">
              {merkleRoot}
            </span>
          </div>
        </div>

        {/* Connector line */}
        {lvl1.length > 0 && (
          <div className="flex justify-center">
            <div className="w-px h-4 bg-slate-700" />
          </div>
        )}

        {/* Level 1 — pairs */}
        {lvl1.length > 0 && (
          <>
            <div className="flex items-start justify-center gap-3">
              {lvl1.map((label, i) => (
                <div key={i} className="flex-1 max-w-[160px]">
                  <div className="bg-slate-800 border border-slate-700 rounded-lg px-2 py-1.5 text-center">
                    <span className="font-mono text-[10px] text-slate-400">
                      {label}
                    </span>
                  </div>
                </div>
              ))}
            </div>

            {/* Connector */}
            <div className="flex justify-center">
              <div className="w-px h-4 bg-slate-700" />
            </div>
          </>
        )}

        {/* Leaves */}
        {visLeaves.length > 0 && (
          <div className="flex items-start justify-center gap-2 flex-wrap">
            {visLeaves.map((h, i) => (
              <div
                key={i}
                className="bg-slate-800/60 border border-slate-700/60 rounded-md px-2 py-1"
              >
                <span className="font-mono text-[10px] text-slate-500">
                  {short(h)}
                </span>
              </div>
            ))}
            {showEllipsis && (
              <div className="bg-slate-800/60 border border-slate-700/60 rounded-md px-2 py-1">
                <span className="text-[10px] text-slate-600">
                  +{leaves.length - 4} more
                </span>
              </div>
            )}
            <div className="w-full text-center">
              <span className="text-slate-600 text-[10px]">
                block hashes (leaves)
              </span>
            </div>
          </div>
        )}
      </div>

      {/* Full root for copy-paste verification */}
      <div className="mt-5 pt-4 border-t border-slate-800">
        <p className="text-slate-600 text-[10px] uppercase tracking-widest mb-1.5">
          Full root hash
        </p>
        <p className="font-mono text-[11px] text-slate-500 break-all leading-relaxed select-all">
          {merkleRoot}
        </p>
      </div>
    </div>
  )
}

// ═══════════════════════════════════════════════════════════
//  Results page
// ═══════════════════════════════════════════════════════════

export default function ResultsPage() {
  const { electionId } = useParams<{ electionId: string }>()
  const router = useRouter()
  const barsRef = useRef<HTMLDivElement>(null)
  const [isPending, startTransition] = useTransition()

  const [results, setResults] = useState<ResultsResponse | null>(null)
  const [electionTitle, setElectionTitle] = useState('')
  const [userVotedFor, setUserVotedFor] = useState<string | null>(null)
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)

  const redirect = useCallback(
    (path: string) => {
      startTransition(() => router.push(path))
    },
    [router]
  )

  const load = useCallback(async () => {
    try {
      setLoading(true)
      setError(null)

      const {
        data: { session },
      } = await supabase.auth.getSession()
      if (!session) return redirect('/')

      const settled = await Promise.allSettled([
        supabase
          .from('elections')
          .select('title')
          .eq('id', electionId)
          .single(),

        supabase
          .from('user_votes')
          .select('candidate_id')
          .eq('user_id', session.user.id)
          .eq('election_id', electionId)
          .maybeSingle(),

        fetch(`/api/results/${electionId}`, {
          headers: { Authorization: `Bearer ${session.access_token}` },
        }),
      ])

      const [electionResult, voteResult, apiResult] = settled

      if (
        electionResult.status !== 'fulfilled' ||
        electionResult.value.error
      ) {
        throw electionResult.status === 'fulfilled'
          ? electionResult.value.error
          : electionResult.reason
      }

      if (voteResult.status !== 'fulfilled' || voteResult.value.error) {
        throw voteResult.status === 'fulfilled'
          ? voteResult.value.error
          : voteResult.reason
      }

      if (apiResult.status !== 'fulfilled') throw apiResult.reason
      if (!apiResult.value.ok) throw new Error('Could not load results')

      const data: ResultsResponse = await apiResult.value.json()

      setElectionTitle(electionResult.value.data?.title ?? '')
      setUserVotedFor(voteResult.value.data?.candidate_id ?? null)
      setResults(data)
    } catch (err) {
      console.error('Failed to load results:', err)
      setError(err instanceof Error ? err.message : 'Failed to load results')
    } finally {
      setLoading(false)
    }
  }, [electionId, redirect])

  useEffect(() => {
    load()
  }, [load])

  useEffect(() => {
    if (!loading && barsRef.current && results) {
      const bars = barsRef.current.querySelectorAll('[data-bar]')
      gsap.fromTo(
        bars,
        { scaleX: 0, transformOrigin: 'left center' },
        { scaleX: 1, duration: 0.8, stagger: 0.12, ease: 'power3.out', delay: 0.2 }
      )
    }
  }, [loading, results])

  if (loading || isPending) {
    return (
      <div className="min-h-screen bg-slate-950 flex items-center justify-center">
        <div className="w-5 h-5 border-2 border-indigo-500 border-t-transparent rounded-full animate-spin" />
      </div>
    )
  }

  if (error || !results) {
    return (
      <div className="min-h-screen bg-slate-950 flex items-center justify-center text-slate-500">
        {error ?? 'No results found'}
      </div>
    )
  }

  const totalVotes = Object.values(results.tally).reduce(
    (a, b) => Number(a) + Number(b),
    0
  )

  const ranked = [...results.candidates].sort(
    (a: Candidate, b: Candidate) =>
      (results.tally[b.id] ?? 0) - (results.tally[a.id] ?? 0)
  )

  const latestHash =
    results.block_count > 1
      ? JSON.parse(results.chain_json).at(-1)?.hash ?? '—'
      : null

  return (
    <div className="min-h-screen bg-slate-950 px-4 py-10">
      <div className="max-w-lg mx-auto">
        <button
          onClick={() => redirect('/dashboard')}
          className="flex items-center gap-1.5 text-slate-500 hover:text-slate-300 text-sm mb-8 transition-colors"
        >
          <RiArrowLeftLine /> Dashboard
        </button>

        <div className="mb-8">
          <span className="text-indigo-400 text-xs font-mono uppercase tracking-widest">
            Live results
          </span>
          <h1 className="text-2xl font-bold text-slate-100 mt-1">
            {electionTitle}
          </h1>
          <p className="text-slate-500 text-sm mt-1">
            {Number(totalVotes)} vote{totalVotes !== 1 ? 's' : ''} recorded
          </p>
        </div>

        {/* Chain validity + block count badges */}
        <div className="flex items-center gap-3 mb-8">
          <div
            className={`flex items-center gap-2 text-sm px-3 py-1.5 rounded-lg border ${
              results.is_valid
                ? 'bg-emerald-500/10 border-emerald-500/20 text-emerald-400'
                : 'bg-red-500/10 border-red-500/20 text-red-400'
            }`}
          >
            {results.is_valid ? (
              <>
                <RiShieldCheckLine /> Chain valid
              </>
            ) : (
              <>
                <RiShieldLine /> Chain tampered!
              </>
            )}
          </div>

          <div className="flex items-center gap-2 text-sm px-3 py-1.5 rounded-lg border border-slate-700 bg-slate-900 text-slate-400">
            <RiDatabase2Line />
            {results.block_count} block{results.block_count !== 1 ? 's' : ''}
          </div>
        </div>

        {/* Vote bars */}
        <div ref={barsRef} className="space-y-5 mb-10">
          {ranked.map((candidate, i) => {
            const votes = results.tally[candidate.id] ?? 0
            const pct =
              Number(totalVotes) > 0
                ? Math.round((Number(votes) / Number(totalVotes)) * 100)
                : 0
            const isLeader = i === 0 && votes > 0
            const isMyVote = candidate.id === userVotedFor

            return (
              <div key={candidate.id}>
                <div className="flex items-center justify-between mb-1.5">
                  <div className="flex items-center gap-2">
                    <span className="font-medium text-slate-200 text-sm">
                      {candidate.name}
                    </span>
                    {isMyVote && (
                      <Badge className="bg-indigo-500/10 text-indigo-400 border-indigo-500/20 text-xs">
                        <RiCheckboxCircleLine className="mr-1" />
                        Your vote
                      </Badge>
                    )}
                    {isLeader && (
                      <Badge className="bg-amber-500/10 text-amber-400 border-amber-500/20 text-xs">
                        Leading
                      </Badge>
                    )}
                  </div>
                  <span className="text-slate-400 text-sm tabular-nums">
                    {votes} · {pct}%
                  </span>
                </div>

                <div className="h-2 bg-slate-800 rounded-full overflow-hidden">
                  <div
                    data-bar
                    className={`h-full rounded-full ${
                      isLeader ? 'bg-indigo-500' : 'bg-slate-600'
                    }`}
                    style={{ width: `${pct}%` }}
                  />
                </div>
              </div>
            )
          })}
        </div>

        {/* Merkle tree verification panel */}
        <MerklePanel
          merkleRoot={results.merkle_root}
          blockCount={results.block_count}
          chainJson={results.chain_json}
        />

        {/* Chain head hash */}
        {latestHash && (
          <div className="bg-slate-900 border border-slate-800 rounded-xl p-4 mt-4">
            <p className="text-slate-600 text-xs mb-2 uppercase tracking-widest">
              Chain head
            </p>
            <p className="font-mono text-xs text-slate-500 break-all leading-relaxed">
              {latestHash}
            </p>
          </div>
        )}
      </div>
    </div>
  )
}