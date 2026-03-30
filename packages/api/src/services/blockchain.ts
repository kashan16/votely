//api/src/services/blockchain.ts
import { spawn } from 'child_process';
import path from 'path';

// Path to your compiled binary
const BINARY = path.resolve(__dirname, '../../../blockchain/build/voting_system');

function runCommand(cmd: string, payload: object = {}): Promise<unknown> {
  return new Promise((resolve, reject) => {
    const args   = [cmd, JSON.stringify(payload)];
    const proc   = spawn(BINARY, args);
    let stdout   = '';
    let stderr   = '';

    proc.stdout.on('data', (d) => (stdout += d.toString()));
    proc.stderr.on('data', (d) => (stderr += d.toString()));

    proc.on('close', (code) => {
      if (stderr) console.error('[blockchain]', stderr.trim());
      try {
        resolve(JSON.parse(stdout.trim()));
      } catch {
        reject(new Error(`Binary output parse failed: ${stdout}`));
      }
    });

    proc.on('error', reject);
  });
}

export const blockchain = {
  castVote: (voterID: string, candidate: string, electionID: string,
             signature: string, publicKey: string) =>
    runCommand('cast', { voterID, candidate, electionID, signature, publicKey }),

  getResults: (electionID: string) =>
    runCommand('results', { electionID }),

  getChain: () =>
    runCommand('chain'),

  validate: () =>
    runCommand('validate'),
};