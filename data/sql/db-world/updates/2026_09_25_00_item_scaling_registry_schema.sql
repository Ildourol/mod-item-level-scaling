--
-- Upgrade scaled_item_variant without changing persisted variant IDs or item values.
-- This file is applied by AzerothCore's module database updater before module startup.
--

CREATE TABLE IF NOT EXISTS `scaled_item_variant` (
    `variant_entry` INT UNSIGNED NOT NULL COMMENT 'Allocated synthetic item entry',
    `base_entry` INT UNSIGNED NOT NULL COMMENT 'Original base ItemTemplate entry',
    `target_effective_level` TINYINT UNSIGNED NOT NULL COMMENT 'Target effective scaling level (1-80)',
    `target_item_level` SMALLINT UNSIGNED NOT NULL COMMENT 'Target ItemLevel calculated from stock baseline',
    `formula_version` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'Scaling formula version used for generation',
    `required_level` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Resolved equip requirement; part of variant identity',
    `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Generation timestamp',
    PRIMARY KEY (`variant_entry`),
    UNIQUE KEY `uk_variant_key` (
        `base_entry`,
        `target_effective_level`,
        `target_item_level`,
        `formula_version`,
        `required_level`
    )
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
COMMENT='Persisted scaled item variants';

ALTER TABLE `scaled_item_variant`
    ADD COLUMN IF NOT EXISTS `required_level` TINYINT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'Resolved equip requirement; part of variant identity'
        AFTER `formula_version`;

UPDATE `scaled_item_variant` s
LEFT JOIN `item_template` i ON i.`entry` = s.`variant_entry`
SET s.`required_level` = LEAST(
    80,
    GREATEST(1, COALESCE(i.`RequiredLevel`, s.`target_effective_level`))
)
WHERE s.`required_level` = 0;

ALTER TABLE `scaled_item_variant`
    DROP INDEX IF EXISTS `uk_variant_key`,
    ADD UNIQUE KEY `uk_variant_key` (
        `base_entry`,
        `target_effective_level`,
        `target_item_level`,
        `formula_version`,
        `required_level`
    );
