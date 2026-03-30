//api/src/routes/chain.ts
import { Request, Response, Router } from 'express';
import { blockchain } from '../services/blockchain';

const router = Router();

// GET /chain           — full chain (for audit/explorer)
// GET /chain/validate  — integrity check
router.get('/', async (_req: Request, res: Response) => {
  const result = await blockchain.getChain() as any;
  if (!result.ok) return res.status(500).json({ error: result.error });
  res.json(result.chain);
});

router.get('/validate', async (_req: Request, res: Response) => {
  const result = await blockchain.validate() as any;
  res.json({ valid: result.valid });
});

export default router;