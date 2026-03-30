//api/src/routes/results.ts
import { Request, Response, Router } from 'express';
import { blockchain } from '../services/blockchain';
import { TallyResult } from '../types';

const router = Router();

// GET /results/:electionId
router.get('/:electionId', async (req: Request, res: Response) => {
  const result = await blockchain.getResults(String(req.params.electionId)) as TallyResult;
  if (!result.ok) return res.status(500).json({ error: result.error });
  res.json(result.tally);
});

export default router;