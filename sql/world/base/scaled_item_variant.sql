--
-- Table structure for table `scaled_item_variant`
-- Permanent scaled item identities and complete generation metadata
--

CREATE TABLE IF NOT EXISTS `scaled_item_variant` (
    `variant_entry` INT UNSIGNED NOT NULL COMMENT 'Allocated synthetic item entry',
    `base_entry` INT UNSIGNED NOT NULL COMMENT 'Original base ItemTemplate entry',
    `target_effective_level` TINYINT UNSIGNED NOT NULL COMMENT 'Target effective scaling level (1-80)',
    `target_item_level` SMALLINT UNSIGNED NOT NULL COMMENT 'Target ItemLevel calculated from stock baseline',
    `formula_version` TINYINT UNSIGNED NOT NULL COMMENT 'Operator-selected scaling formula family',
    `generator_revision` TINYINT UNSIGNED NOT NULL COMMENT 'Internal template generator revision',
    `required_level` TINYINT UNSIGNED NOT NULL COMMENT 'Resolved equip requirement; part of variant identity',
    `random_property_id` INT NOT NULL DEFAULT 0 COMMENT 'Rolled random property or suffix ID (0 for fixed-stat or skipped items)',
    `base_class` TINYINT UNSIGNED NOT NULL,
    `base_subclass` TINYINT UNSIGNED NOT NULL,
    `base_sound_override_subclass` TINYINT NOT NULL,
    `base_material` TINYINT NOT NULL,
    `base_displayid` INT UNSIGNED NOT NULL,
    `base_inventory_type` TINYINT UNSIGNED NOT NULL,
    `base_sheath` TINYINT UNSIGNED NOT NULL,
    `preserve_nonzero_stats` TINYINT UNSIGNED NOT NULL,
    `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Generation timestamp',
    PRIMARY KEY (`variant_entry`),
    UNIQUE KEY `uk_variant_key` (
        `base_entry`,
        `target_effective_level`,
        `target_item_level`,
        `formula_version`,
        `generator_revision`,
        `required_level`,
        `random_property_id`
    )
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Persisted scaled item variants';
