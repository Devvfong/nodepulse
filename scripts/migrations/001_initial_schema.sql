-- Phase 14 Database Schema
-- Idempotent initial schema migration for NodePulse host metric history

CREATE TABLE IF NOT EXISTS host_metrics (
    id BIGSERIAL PRIMARY KEY,
    captured_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    hostname VARCHAR(255) NOT NULL,
    cpu_usage_percent DOUBLE PRECISION,
    load_1m DOUBLE PRECISION NOT NULL,
    load_5m DOUBLE PRECISION NOT NULL,
    load_15m DOUBLE PRECISION NOT NULL,
    mem_total_bytes BIGINT NOT NULL,
    mem_used_bytes BIGINT NOT NULL,
    mem_available_bytes BIGINT NOT NULL,
    swap_total_bytes BIGINT NOT NULL,
    swap_used_bytes BIGINT NOT NULL,
    net_rx_bytes_per_sec DOUBLE PRECISION,
    net_tx_bytes_per_sec DOUBLE PRECISION
);

CREATE INDEX IF NOT EXISTS idx_host_metrics_hostname_time
    ON host_metrics (hostname, captured_at DESC);
