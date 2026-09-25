--
-- Table structure for table `scaled_item_variant`
-- Stores dynamically generated scaled item variants persisted across restarts
--

CREATE TABLE IF NOT EXISTS `scaled_item_variant` (
    `variant_entry` INT UNSIGNED NOT NULL COMMENT 'Allocated synthetic item entry',
    `base_entry` INT UNSIGNED NOT NULL COMMENT 'Original base ItemTemplate entry',
    `target_effective_level` TINYINT UNSIGNED NOT NULL COMMENT 'Target effective scaling level (1-80)',
    `target_item_level` SMALLINT UNSIGNED NOT NULL COMMENT 'Target ItemLevel calculated from stock baseline',
    `formula_version` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'Scaling formula version used for generation',
    `required_level` TINYINT UNSIGNED NOT NULL COMMENT 'Resolved equip requirement; part of variant identity',
    `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Generation timestamp',
    PRIMARY KEY (`variant_entry`),
    UNIQUE KEY `uk_variant_key` (`base_entry`, `target_effective_level`, `target_item_level`, `formula_version`, `required_level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Persisted scaled item variants';
