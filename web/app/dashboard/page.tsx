'use client'

// app/dashboard/page.tsx
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import { supabase } from '@/lib/supabase'
import { Election, UserVote } from '@/types'

import { gsap } from 'gsap'
import { useRouter } from 'next/navigation'
import { useCallback, useEffect, useRef, useState } from 'react'
import {
    RiArrowRightLine,
    RiCheckboxCircleLine,
    RiInformationLine,
    RiLogoutBoxLine,
    RiShieldCheckLine,
    RiTimeLine,
} from 'react-icons/ri'

export default function DashboardPage() {
  const router = useRouter()
  const cardsRef = useRef<HTMLDivElement>(null)

  const [elections, setElections] = useState<Election[]>([])
  const [votedIds, setVotedIds] = useState<Set<string>>(new Set())
  const [loading, setLoading] = useState(true)
  const [userId, setUserId] = useState<string | null>(null)

  const load = useCallback(async () => {
    const { data: { session } } = await supabase.auth.getSession()
    if (!session) { router.push('/'); return }
    setUserId(session.user.id)

    const [{ data: elections }, { data: votes }] = await Promise.all([
      supabase.from('elections').select('*').eq('is_active', true).order('created_at', { ascending: false }),
      supabase.from('user_votes').select('election_id').eq('user_id', session.user.id),
    ])

    setElections(elections ?? [])
    setVotedIds(new Set((votes as UserVote[] ?? []).map(v => v.election_id)))
    setLoading(false)
  }, [router])

  // eslint-disable-next-line react-hooks/set-state-in-effect
  useEffect(() => { load() }, [load])

  // GSAP stagger once cards are loaded
  useEffect(() => {
    if (!loading && cardsRef.current) {
      gsap.fromTo(
        cardsRef.current.children,
        { opacity: 0, y: 20 },
        { opacity: 1, y: 0, duration: 0.5, stagger: 0.1, ease: 'power2.out' },
      )
    }
  }, [loading])

  async function signOut() {
    await supabase.auth.signOut()
    router.push('/')
  }

  const timeLeft = (end: string) => {
    // eslint-disable-next-line react-hooks/purity
    const diff = new Date(end).getTime() - Date.now()
    if (diff <= 0) return 'Ended'
    const days = Math.floor(diff / 86400000)
    const hrs  = Math.floor((diff % 86400000) / 3600000)
    return days > 0 ? `${days}d ${hrs}h left` : `${hrs}h left`
  }

  return (
    <div className="min-h-screen bg-slate-950">
      {/* Nav */}
      <header className="border-b border-slate-800 bg-slate-950/80 backdrop-blur sticky top-0 z-10">
        <div className="max-w-3xl mx-auto px-4 h-14 flex items-center justify-between">
          <div className="flex items-center gap-2 text-slate-100 font-semibold">
            <RiShieldCheckLine className="text-indigo-400 text-lg" />
            VoteX
          </div>
          <div className="flex items-center gap-3">
            <span className="text-slate-500 text-sm hidden sm:block">{userId?.slice(0, 8)}…</span>
            <Button variant="ghost" size="sm" onClick={signOut} className="text-slate-400 hover:text-slate-100">
              <RiLogoutBoxLine className="mr-1.5" /> Sign out
            </Button>
          </div>
        </div>
      </header>

      <main className="max-w-3xl mx-auto px-4 py-10">
        <div className="mb-8">
          <h1 className="text-2xl font-bold text-slate-100">Active Elections</h1>
          <p className="text-slate-500 text-sm mt-1">Each vote is recorded as an immutable block on-chain.</p>
        </div>

        {loading && (
          <div className="space-y-3">
            {[1, 2].map(i => (
              <div key={i} className="h-28 rounded-xl bg-slate-900 border border-slate-800 animate-pulse" />
            ))}
          </div>
        )}

        {!loading && elections.length === 0 && (
          <div className="text-center py-16 text-slate-500">
            <RiInformationLine className="text-3xl mx-auto mb-3" />
            <p>No active elections right now.</p>
          </div>
        )}

        {!loading && (
          <div ref={cardsRef} className="space-y-3">
            {elections.map(election => {
              const voted = votedIds.has(election.id)
              return (
                <div
                  key={election.id}
                  className="bg-slate-900 border border-slate-800 rounded-xl p-5 flex items-start justify-between gap-4 hover:border-slate-700 transition-colors"
                >
                  <div className="flex-1 min-w-0">
                    <div className="flex items-center gap-2 mb-1">
                      <h2 className="font-semibold text-slate-100 truncate">{election.title}</h2>
                      {voted && (
                        <Badge className="bg-emerald-500/10 text-emerald-400 border-emerald-500/20 text-xs">
                          <RiCheckboxCircleLine className="mr-1" /> Voted
                        </Badge>
                      )}
                    </div>
                    {election.description && (
                      <p className="text-slate-500 text-sm truncate">{election.description}</p>
                    )}
                    <p className="text-slate-600 text-xs mt-2 flex items-center gap-1">
                      <RiTimeLine /> {timeLeft(election.end_at)}
                    </p>
                  </div>

                  <div className="flex flex-col gap-2 shrink-0">
                    {!voted && (
                      <Button
                        size="sm"
                        onClick={() => router.push(`/vote/${election.id}`)}
                        className="bg-indigo-600 hover:bg-indigo-500 text-white text-xs"
                      >
                        Vote <RiArrowRightLine className="ml-1" />
                      </Button>
                    )}
                    <Button
                      size="sm"
                      variant="outline"
                      onClick={() => router.push(`/results/${election.id}`)}
                      className="border-slate-700 text-slate-400 hover:text-slate-100 text-xs"
                    >
                      Results
                    </Button>
                  </div>
                </div>
              )
            })}
          </div>
        )}
      </main>
    </div>
  )
}