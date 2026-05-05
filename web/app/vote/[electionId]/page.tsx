'use client'

import { Button } from '@/components/ui/button'
import { supabase } from '@/lib/supabase'
import { Candidate, Election } from '@/types'

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
    RiCheckLine,
    RiShieldCheckLine,
} from 'react-icons/ri'

export default function VotePage() {
  const { electionId } = useParams<{ electionId: string }>()
  const router = useRouter()
  const optionsRef = useRef<HTMLDivElement>(null)
  const [isPending, startTransition] = useTransition()

  const [election, setElection] = useState<Election | null>(null)
  const [candidates, setCandidates] = useState<Candidate[]>([])
  const [selected, setSelected] = useState<string | null>(null)
  const [loading, setLoading] = useState(true)
  const [submitting, setSubmitting] = useState(false)
  const [error, setError] = useState<string | null>(null)

  const redirect = useCallback(
    (path: string) => {
      startTransition(() => router.push(path))
    },
    [router]
  )

  const getSession = async () => {
    const {
      data: { session },
    } = await supabase.auth.getSession()

    return session
  }

  const load = useCallback(async () => {
    try {
      setLoading(true)
      setError(null)

      const session = await getSession()
      if (!session) return redirect('/')

      const { data: existing, error: voteCheckError } = await supabase
        .from('user_votes')
        .select('id')
        .eq('user_id', session.user.id)
        .eq('election_id', electionId)
        .maybeSingle()

      if (voteCheckError) throw voteCheckError
      if (existing) return redirect(`/results/${electionId}`)

      const results = await Promise.allSettled([
        supabase.from('elections').select('*').eq('id', electionId).single(),
        supabase
          .from('candidates')
          .select('*')
          .eq('election_id', electionId)
          .order('name'),
      ])

      const [electionResult, candidatesResult] = results

      if (
        electionResult.status !== 'fulfilled' ||
        electionResult.value.error
      ) {
        throw electionResult.status === 'fulfilled'
          ? electionResult.value.error
          : electionResult.reason
      }

      if (
        candidatesResult.status !== 'fulfilled' ||
        candidatesResult.value.error
      ) {
        throw candidatesResult.status === 'fulfilled'
          ? candidatesResult.value.error
          : candidatesResult.reason
      }

      setElection(electionResult.value.data)
      setCandidates(candidatesResult.value.data ?? [])
    } catch (err) {
      console.error('Failed to load election:', err)
      setError('Failed to load election data')
    } finally {
      setLoading(false)
    }
  }, [electionId, redirect])

  useEffect(() => {
    // eslint-disable-next-line react-hooks/set-state-in-effect
    load()
  }, [load])

  useEffect(() => {
    if (!loading && optionsRef.current) {
      gsap.fromTo(
        optionsRef.current.children,
        { opacity: 0, x: -16 },
        {
          opacity: 1,
          x: 0,
          duration: 0.4,
          stagger: 0.08,
          ease: 'power2.out',
        }
      )
    }
  }, [loading])

  const castVote = async () => {
    if (!selected) return

    try {
      setSubmitting(true)
      setError(null)

      const session = await getSession()
      if (!session) return redirect('/')

      const res = await fetch('/api/vote', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          Authorization: `Bearer ${session.access_token}`,
        },
        body: JSON.stringify({
          election_id: electionId,
          candidate_id: selected,
        }),
      })

      const body = await res.json()

      if (!res.ok) {
        throw new Error(body.error || 'Something went wrong')
      }

      gsap.to(optionsRef.current, {
        opacity: 0,
        y: -10,
        duration: 0.3,
        onComplete: () => redirect(`/results/${electionId}`),
      })
    } catch (err) {
      console.error('Vote failed:', err)
      setError(err instanceof Error ? err.message : 'Vote submission failed')
    } finally {
      setSubmitting(false)
    }
  }

  if (loading || isPending) {
    return (
      <div className="min-h-screen bg-slate-950 flex items-center justify-center">
        <div className="w-5 h-5 border-2 border-indigo-500 border-t-transparent rounded-full animate-spin" />
      </div>
    )
  }

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
          <div className="flex items-center gap-2 mb-1">
            <RiShieldCheckLine className="text-indigo-400" />
            <span className="text-indigo-400 text-xs font-mono uppercase tracking-widest">
              Cast your vote
            </span>
          </div>
          <h1 className="text-2xl font-bold text-slate-100">
            {election?.title}
          </h1>
          {election?.description && (
            <p className="text-slate-500 text-sm mt-2">
              {election.description}
            </p>
          )}
        </div>

        <div ref={optionsRef} className="space-y-3 mb-8">
          {candidates.map((c) => (
            <button
              key={c.id}
              onClick={() => setSelected(c.id)}
              className={`w-full text-left rounded-xl border p-4 transition-all duration-200 ${
                selected === c.id
                  ? 'border-indigo-500 bg-indigo-500/10 shadow-lg shadow-indigo-500/10'
                  : 'border-slate-800 bg-slate-900 hover:border-slate-700'
              }`}
            >
              <div className="flex items-center justify-between">
                <div>
                  <p className="font-semibold text-slate-100">{c.name}</p>
                  {c.description && (
                    <p className="text-slate-500 text-sm mt-0.5">
                      {c.description}
                    </p>
                  )}
                </div>

                <div
                  className={`w-5 h-5 rounded-full border-2 flex items-center justify-center shrink-0 ml-4 transition-colors ${
                    selected === c.id
                      ? 'border-indigo-400 bg-indigo-500'
                      : 'border-slate-600'
                  }`}
                >
                  {selected === c.id && (
                    <RiCheckLine className="text-white text-xs" />
                  )}
                </div>
              </div>
            </button>
          ))}
        </div>

        {error && (
          <p className="text-red-400 text-sm bg-red-500/10 border border-red-500/20 rounded-lg px-3 py-2 mb-4">
            {error}
          </p>
        )}

        <Button
          onClick={castVote}
          disabled={!selected || submitting}
          className="w-full bg-indigo-600 hover:bg-indigo-500 text-white font-medium disabled:opacity-40 transition-colors"
        >
          {submitting ? (
            <span className="flex items-center gap-2">
              <span className="w-4 h-4 border-2 border-white/30 border-t-white rounded-full animate-spin" />
              Recording on blockchain…
            </span>
          ) : (
            'Confirm Vote'
          )}
        </Button>

        <p className="text-center text-slate-600 text-xs mt-4">
          Your vote is final and immutable once confirmed.
        </p>
      </div>
    </div>
  )
}