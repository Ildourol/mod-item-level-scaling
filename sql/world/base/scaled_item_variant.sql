--
-- Table structure for table `scaled_item_variant`
-- Stores permanent scaled item identities and recovery metadata
--

CREATE TABLE IF NOT EXISTS `scaled_item_variant` (
    `variant_entry` INT UNSIGNED NOT NULL COMMENT 'Allocated synthetic item entry',
    `base_entry` INT UNSIGNED NOT NULL COMMENT 'Original base ItemTemplate entry',
    `target_effective_level` TINYINT UNSIGNED NOT NULL COMMENT 'Target effective scaling level (1-80)',
    `target_item_level` SMALLINT UNSIGNED NOT NULL COMMENT 'Target ItemLevel calculated from stock baseline',
    `formula_version` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'Operator-selected scaling formula family',
    `generator_revision` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'Internal template generator revision',
    `required_level` TINYINT UNSIGNED NOT NULL COMMENT 'Resolved equip requirement; part of variant identity',
    `base_class` TINYINT UNSIGNED NULL DEFAULT NULL,
    `base_subclass` TINYINT UNSIGNED NULL DEFAULT NULL,
    `base_sound_override_subclass` TINYINT NULL DEFAULT NULL,
    `base_material` TINYINT NULL DEFAULT NULL,
    `base_displayid` INT UNSIGNED NULL DEFAULT NULL,
    `base_inventory_type` TINYINT UNSIGNED NULL DEFAULT NULL,
    `base_sheath` TINYINT UNSIGNED NULL DEFAULT NULL,
    `preserve_nonzero_stats` TINYINT UNSIGNED NULL DEFAULT NULL,
    `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Generation timestamp',
    PRIMARY KEY (`variant_entry`),
    UNIQUE KEY `uk_variant_key` (
        `base_entry`,
        `target_effective_level`,
        `target_item_level`,
        `formula_version`,
        `generator_revision`,
        `required_level`
    )
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Persisted scaled item variants';

-- Pending requests have no allocated synthetic entry.
CREATE TABLE IF NOT EXISTS `scaled_item_variant_request` (
    `base_entry` INT UNSIGNED NOT NULL,
    `target_effective_level` TINYINT UNSIGNED NOT NULL,
    `target_item_level` SMALLINT UNSIGNED NOT NULL,
    `formula_version` TINYINT UNSIGNED NOT NULL,
    `generator_revision` TINYINT UNSIGNED NOT NULL,
    `required_level` TINYINT UNSIGNED NOT NULL,
    `base_class` TINYINT UNSIGNED NOT NULL,
    `base_subclass` TINYINT UNSIGNED NOT NULL,
    `base_sound_override_subclass` TINYINT NOT NULL,
    `base_material` TINYINT NOT NULL,
    `base_displayid` INT UNSIGNED NOT NULL,
    `base_inventory_type` TINYINT UNSIGNED NOT NULL,
    `base_sheath` TINYINT UNSIGNED NOT NULL,
    `preserve_nonzero_stats` TINYINT UNSIGNED NOT NULL,
    `requested_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (
        `base_entry`,
        `target_effective_level`,
        `target_item_level`,
        `formula_version`,
        `generator_revision`,
        `required_level`
    )
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Exact gameplay scaling demand';
