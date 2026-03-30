export interface CastVoteRequest {
    electionID: string;
    candidate:  string;
    signature:  string;   // hex-encoded, signed on the frontend
    publicKey:  string;   // PEM, stored in Supabase voters table
}

export interface BlockchainResult {
    ok:        boolean;
    blockHash?: string;
    error?:    string;
}

export interface TallyResult {
    ok:     boolean;
    tally?: Record<string, number>;
    error?: string;
}

export interface AuthenticatedRequest extends Express.Request {
    voterID: string;
}