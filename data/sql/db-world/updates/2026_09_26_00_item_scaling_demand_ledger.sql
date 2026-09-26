-- Add nullable recovery metadata without changing any issued entry, key, or scaled value.
-- NULL preserves the distinction between legacy unknown metadata and a captured runtime snapshot.
ALTER TABLE `scaled_item_variant`
    ADD COLUMN IF NOT EXISTS `base_class` TINYINT UNSIGNED NULL DEFAULT NULL,
    ADD COLUMN IF NOT EXISTS `base_subclass` TINYINT UNSIGNED NULL DEFAULT NULL,
    ADD COLUMN IF NOT EXISTS `base_sound_override_subclass` TINYINT NULL DEFAULT NULL,
    ADD COLUMN IF NOT EXISTS `base_material` TINYINT NULL DEFAULT NULL,
    ADD COLUMN IF NOT EXISTS `base_displayid` INT UNSIGNED NULL DEFAULT NULL,
    ADD COLUMN IF NOT EXISTS `base_inventory_type` TINYINT UNSIGNED NULL DEFAULT NULL,
    ADD COLUMN IF NOT EXISTS `base_sheath` TINYINT UNSIGNED NULL DEFAULT NULL,
    ADD COLUMN IF NOT EXISTS `preserve_nonzero_stats` TINYINT UNSIGNED NULL DEFAULT NULL;

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
