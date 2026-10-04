--
-- Safely retire legacy demand ledger table
-- Rows in scaled_item_variant_request represent pending, unmaterialized requests only (not player-owned items).
-- Active scaling operates via Pure Live Generation with durable staging tables.
--

DROP TABLE IF EXISTS `scaled_item_variant_request`;
