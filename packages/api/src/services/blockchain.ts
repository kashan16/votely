//api/src/services/blockchain.ts
import { spawn } from 'child_process';
import path from 'path';

// Path to your compiled binary.
// Resolved at startup — log it so you can verify it during development.
const BINARY = path.resolve(__dirname, '../../../blockchain/build/voting_system');
console.log('[blockchain] binary path:', BINARY);

function runCommand(cmd: string, payload: object = {}): Promise<unknown> {
  return new Promise((resolve, reject) => {
    const args  = [cmd, JSON.stringify(payload)];
    const proc  = spawn(BINARY, args);
    let stdout  = '';
    let stderr  = '';

    proc.stdout.on('data', (d) => (stdout += d.toString()));
    proc.stderr.on('data', (d) => (stderr += d.toString()));

    proc.on('close', (code) => {
      if (stderr) console.error('[blockchain stderr]', stderr.trim());

      // Non-zero exit means the binary itself reported a failure.
      if (code !== 0) {
        return reject(
          new Error(
            `Blockchain binary exited with code ${code}.` +
            (stderr ? ` stderr: ${stderr.trim()}` : '') +
            (stdout ? ` stdout: ${stdout.trim()}` : '')
          )
        );
      }

      if (!stdout.trim()) {
        return reject(new Error(`Blockchain binary produced no output for command "${cmd}"`));
      }

      // The binary writes diagnostic log lines (e.g. [persist], [miner]) to stdout
      // alongside the final JSON result. Extract only the last non-empty line,
      // which is always the JSON payload.
      const lines = stdout
        .split('\n')
        .map((l) => l.trim())
        .filter(Boolean);

      const jsonLine = lines[lines.length - 1];

      try {
        resolve(JSON.parse(jsonLine));
      } catch {
        reject(
          new Error(
            `Failed to parse blockchain output for command "${cmd}": ${stdout.trim()}`
          )
        );
      }
    });

    // Handles ENOENT (binary not found), EACCES (not executable), etc.
    proc.on('error', (err) => {
      reject(
        new Error(
          `Failed to spawn blockchain binary at "${BINARY}": ${err.message}`
        )
      );
    });
  });
}

export const blockchain = {
  castVote: (
    voterID: string,
    candidate: string,
    electionID: string,
    signature: string,
    publicKey: string
  ) => runCommand('cast', { voterID, candidate, electionID, signature, publicKey }),

  getResults: (electionID: string) => runCommand('results', { electionID }),

  getChain: () => runCommand('chain'),

  validate: () => runCommand('validate'),
};