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

-- Pending requests have no allocated synthetic entry.
CREATE TABLE IF NOT EXISTS `scaled_item_variant_request` (
    `base_entry` INT UNSIGNED NOT NULL,
    `target_effective_level` TINYINT UNSIGNED NOT NULL,
    `target_item_level` SMALLINT UNSIGNED NOT NULL,
    `formula_version` TINYINT UNSIGNED NOT NULL,
    `generator_revision` TINYINT UNSIGNED NOT NULL,
    `required_level` TINYINT UNSIGNED NOT NULL,
    `random_property_id` INT NOT NULL DEFAULT 0,
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
        `required_level`,
        `random_property_id`
    )
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Exact gameplay scaling demand';

-- Idempotent upgrades for existing legacy tables, using MySQL-compatible guards.
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant' AND COLUMN_NAME='generator_revision'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant` ADD COLUMN `generator_revision` TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER `formula_version`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant' AND COLUMN_NAME='required_level'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant` ADD COLUMN `required_level` TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER `generator_revision`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant' AND COLUMN_NAME='random_property_id'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant` ADD COLUMN `random_property_id` INT NOT NULL DEFAULT 0 AFTER `required_level`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant' AND COLUMN_NAME='base_class'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant` ADD COLUMN `base_class` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `random_property_id`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant' AND COLUMN_NAME='base_subclass'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant` ADD COLUMN `base_subclass` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_class`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant' AND COLUMN_NAME='base_sound_override_subclass'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant` ADD COLUMN `base_sound_override_subclass` TINYINT NOT NULL DEFAULT -1 AFTER `base_subclass`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant' AND COLUMN_NAME='base_material'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant` ADD COLUMN `base_material` TINYINT NOT NULL DEFAULT 0 AFTER `base_sound_override_subclass`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant' AND COLUMN_NAME='base_displayid'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant` ADD COLUMN `base_displayid` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_material`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant' AND COLUMN_NAME='base_inventory_type'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant` ADD COLUMN `base_inventory_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_displayid`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant' AND COLUMN_NAME='base_sheath'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant` ADD COLUMN `base_sheath` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_inventory_type`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant' AND COLUMN_NAME='preserve_nonzero_stats'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant` ADD COLUMN `preserve_nonzero_stats` TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER `base_sheath`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant_request' AND COLUMN_NAME='random_property_id'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant_request` ADD COLUMN `random_property_id` INT NOT NULL DEFAULT 0 AFTER `required_level`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant_request' AND COLUMN_NAME='base_class'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant_request` ADD COLUMN `base_class` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `random_property_id`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant_request' AND COLUMN_NAME='base_subclass'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant_request` ADD COLUMN `base_subclass` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_class`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant_request' AND COLUMN_NAME='base_sound_override_subclass'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant_request` ADD COLUMN `base_sound_override_subclass` TINYINT NOT NULL DEFAULT -1 AFTER `base_subclass`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant_request' AND COLUMN_NAME='base_material'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant_request` ADD COLUMN `base_material` TINYINT NOT NULL DEFAULT 0 AFTER `base_sound_override_subclass`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant_request' AND COLUMN_NAME='base_displayid'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant_request` ADD COLUMN `base_displayid` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_material`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant_request' AND COLUMN_NAME='base_inventory_type'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant_request` ADD COLUMN `base_inventory_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_displayid`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant_request' AND COLUMN_NAME='base_sheath'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant_request` ADD COLUMN `base_sheath` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_inventory_type`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
SET @item_scaling_ddl = IF(EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant_request' AND COLUMN_NAME='preserve_nonzero_stats'), 'SELECT 1', 'ALTER TABLE `scaled_item_variant_request` ADD COLUMN `preserve_nonzero_stats` TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER `base_sheath`');
PREPARE item_scaling_upgrade FROM @item_scaling_ddl;
EXECUTE item_scaling_upgrade;
DEALLOCATE PREPARE item_scaling_upgrade;
