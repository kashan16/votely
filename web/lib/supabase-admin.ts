// lib/supabase-admin.ts  — server client (service role, bypasses RLS)
// NEVER import this in client components or pages.

import { Database } from '@/types'
import { createClient } from '@supabase/supabase-js'

export function supabaseAdmin() {
  const url = process.env.NEXT_PUBLIC_SUPABASE_URL!
  const key = process.env.SUPABASE_SERVICE_ROLE_KEY!
  return createClient<Database>(url, key, {
    auth: { persistSession: false },
  })
}