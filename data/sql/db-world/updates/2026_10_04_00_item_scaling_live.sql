-- Durable staging for runtime templates. This migration never edits issued items.
CREATE TABLE IF NOT EXISTS `mod_item_level_scaling_slot` (
  `entry` INT UNSIGNED NOT NULL,
  `assigned` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `mod_item_level_scaling_staged_item` LIKE `item_template`;
CREATE TABLE IF NOT EXISTS `mod_item_level_scaling_staged_variant` LIKE `scaled_item_variant`;
