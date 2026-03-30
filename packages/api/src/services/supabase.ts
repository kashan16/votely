//api/src/services/supabase.ts
import { createClient } from '@supabase/supabase-js';

if (!process.env.SUPABASE_URL || !process.env.SUPABASE_SERVICE_ROLE_KEY)
  throw new Error('Missing Supabase env vars');

export const supabase = createClient(
  process.env.SUPABASE_URL,
  process.env.SUPABASE_SERVICE_ROLE_KEY   // service role — never exposed to client
);

export async function getVoterPublicKey(voterID: string): Promise<string | null> {
  const { data, error } = await supabase
    .from('voters')
    .select('public_key')
    .eq('id', voterID)
    .single();
  if (error || !data) return null;
  return data.public_key;
}

export async function recordVoteReceipt(
  voterID: string, electionID: string,
  blockHash: string, txSignature: string
) {
  const { error } = await supabase
    .from('vote_receipts')
    .insert({ voter_id: voterID, election_id: electionID, block_hash: blockHash, tx_signature: txSignature });
  if (error) throw new Error(error.message);
}