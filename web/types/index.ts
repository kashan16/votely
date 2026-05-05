// types/index.ts

export type Json =
  | string
  | number
  | boolean
  | null
  | { [key: string]: Json | undefined }
  | Json[]

export type Election = {
  id: string
  title: string
  description: string | null
  start_at: string
  end_at: string
  is_active: boolean
  created_at: string
}

export type Candidate = {
  id: string
  election_id: string
  name: string
  description: string | null
}

export type UserVote = {
  id: string
  user_id: string
  election_id: string
  candidate_id: string
  voted_at: string
}

export type BlockchainState = {
  id: string
  election_id: string
  chain_json: string
  block_count: number
  is_valid: boolean
  last_updated: string
}

export type TallyResult = Record<string, number>

export type ResultsResponse = {
  tally: TallyResult
  candidates: Candidate[]
  block_count: number
  is_valid: boolean
  chain_json: string
  /** SHA-256 Merkle root of all block hashes — a single integrity fingerprint */
  merkle_root: string
}

type Empty = Record<string, never>

export type Database = {
  public: {
    Tables: {
      elections: {
        Row: Election
        Insert: Omit<Election, 'id' | 'created_at'>
        Update: Partial<Omit<Election, 'id' | 'created_at'>>
        Relationships: []
      }

      candidates: {
        Row: Candidate
        Insert: Omit<Candidate, 'id'>
        Update: Partial<Omit<Candidate, 'id'>>
        Relationships: []
      }

      user_votes: {
        Row: UserVote
        Insert: Omit<UserVote, 'id' | 'voted_at'>
        Update: Partial<Omit<UserVote, 'id' | 'voted_at'>>
        Relationships: []
      }

      blockchain_state: {
        Row: BlockchainState
        Insert: Omit<BlockchainState, 'id' | 'last_updated'>
        Update: Partial<Omit<BlockchainState, 'id' | 'last_updated'>>
        Relationships: []
      }
    }

    Views: Empty
    Functions: Empty
    Enums: Empty
    CompositeTypes: Empty
  }
}