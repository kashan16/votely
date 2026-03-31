//api/src/routes/chain.ts
import { Request, Response, Router } from 'express';
import { blockchain } from '../services/blockchain';

const router = Router();

// GET /chain  — full chain (for audit/explorer)
router.get('/', async (_req: Request, res: Response) => {
  try {
    const result = await blockchain.getChain() as any;
    if (!result.ok) return res.status(500).json({ error: result.error });
    res.json(result.chain);
  } catch (err) {
    console.error('[GET /chain]', err);
    res.status(500).json({ error: (err as Error).message });
  }
});

// GET /chain/validate  — integrity check
router.get('/validate', async (_req: Request, res: Response) => {
  try {
    const result = await blockchain.validate() as any;
    res.json({ valid: result.valid });
  } catch (err) {
    console.error('[GET /chain/validate]', err);
    res.status(500).json({ error: (err as Error).message });
  }
});

export default router;