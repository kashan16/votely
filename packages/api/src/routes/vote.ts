//api/src/routes/vote.ts
import { Request, Response, Router } from 'express';
import { requireAuth } from '../middleware/auth';
import { blockchain } from '../services/blockchain';
import { getVoterPublicKey, recordVoteReceipt } from '../services/supabase';
import { BlockchainResult, CastVoteRequest } from '../types';

const router = Router();

// POST /vote
router.post('/', requireAuth, async (req: Request, res: Response) => {
  try {
    const voterID = (req as any).voterID as string;
    const { electionID, candidate, signature } = req.body as CastVoteRequest;

    if (!electionID || !candidate || !signature) {
      return res.status(400).json({ error: 'Missing required fields' });
    }

    // Fetch voter's public key from Supabase.
    // Returns null if the user authenticated successfully but was never inserted
    // into public.voters (i.e. key-pair generation was skipped at sign-up).
    const publicKey = await getVoterPublicKey(voterID);
    if (!publicKey) {
      return res.status(403).json({
        error: 'Voter not registered. Complete voter registration to receive a key pair.',
      });
    }

    // Call C++ binary
    const result = await blockchain.castVote(
      voterID, candidate, electionID, signature, publicKey
    ) as BlockchainResult;

    if (!result.ok) {
      const status = result.error === 'double_vote' ? 409 : 400;
      return res.status(status).json({ error: result.error });
    }

    // Store receipt in Supabase (off-chain record)
    await recordVoteReceipt(voterID, electionID, result.blockHash!, signature);

    res.json({ success: true, blockHash: result.blockHash });
  } catch (err) {
    console.error('[POST /vote]', err);
    res.status(500).json({ error: (err as Error).message });
  }
});

export default router;