import dotenv from "dotenv";
dotenv.config(); // MUST be first

import cors from 'cors';
import express from 'express';
import helmet from 'helmet';

import chainRouter from './routes/chain';
import resultsRouter from './routes/results';
import voteRouter from './routes/vote';

const app  = express();
const PORT = process.env.PORT || 3001;

app.use(helmet());
app.use(cors({ origin: process.env.FRONTEND_URL || 'http://localhost:3000' }));
app.use(express.json());

app.get('/health', (_req, res) => res.json({ status: 'ok' }));

app.use('/vote',    voteRouter);
app.use('/results', resultsRouter);
app.use('/chain',   chainRouter);

app.listen(PORT, () =>
  console.log(`[api] Bridge listening on http://localhost:${PORT}`)
);