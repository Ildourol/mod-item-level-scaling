--
-- Track the code generator revision separately from the operator FormulaVersion.
-- Revision 1 maps every existing row to the generator behavior already in use.
-- No synthetic item IDs or item_template values are changed by this migration.
--

ALTER TABLE `scaled_item_variant`
    ADD COLUMN IF NOT EXISTS `generator_revision` TINYINT UNSIGNED NOT NULL DEFAULT 1
        COMMENT 'Internal template generator revision'
        AFTER `formula_version`;

ALTER TABLE `scaled_item_variant`
    DROP INDEX IF EXISTS `uk_variant_key`,
    ADD UNIQUE KEY `uk_variant_key` (
        `base_entry`,
        `target_effective_level`,
        `target_item_level`,
        `formula_version`,
        `generator_revision`,
        `required_level`
    );
