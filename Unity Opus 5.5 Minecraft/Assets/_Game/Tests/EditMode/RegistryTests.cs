using NUnit.Framework;
using UnityEngine;

namespace MCR.Tests
{
    /// <summary>
    /// Edit-mode checks of the game's data layer: registries, recipes, item persistence, structure placement
    /// rules and texture coverage. Run from the command line with
    /// <c>Unity.exe -batchmode -projectPath . -runTests -testPlatform EditMode -testResults Tools/_out/editmode-results.xml</c>.
    /// </summary>
    public class RegistryTests
    {
        [OneTimeSetUp]
        public void Init()
        {
            Biome.Init();
            Blocks.Init();
            Items.Init();
            Tags.Init();
            Recipes.Init();
            StructureManager.Init();
        }

        [Test]
        public void BlockAndItemRegistriesArePopulated()
        {
            Assert.Greater(Blocks.All.Count, 800, "block registry");
            Assert.Greater(Items.All.Count, 1000, "item registry");
            Assert.IsNotNull(Blocks.Get("stone"));
            Assert.IsNotNull(Blocks.Get("obsidian"));
            Assert.IsNotNull(Items.Get("diamond_pickaxe"));
            Assert.IsNotNull(Items.Get("elytra"));
        }

        [Test]
        public void EveryMobAndBossHasASpawnEgg()
        {
            foreach (var def in MobRegistry.All)
                Assert.IsNotNull(Items.Get(def.id + "_spawn_egg"), "spawn egg for " + def.id);
            Assert.IsNotNull(Items.Get("ender_dragon_spawn_egg"));
            Assert.IsNotNull(Items.Get("wither_spawn_egg"));
        }

        [Test]
        public void OneLogCraftsFourPlanks()
        {
            var grid = new CraftingGrid(2, 2);
            grid.items[0] = new ItemStack("oak_log", 1);
            var r = Recipes.FindCrafting(grid, null);
            Assert.IsNotNull(r, "log to planks recipe");
            var result = r.Assemble(grid);
            Assert.AreEqual("oak_planks", result.item.id);
            Assert.AreEqual(4, result.count);
        }

        [Test]
        public void CraftingTableAndFurnaceRecipesExist()
        {
            var grid = new CraftingGrid(2, 2);
            for (int i = 0; i < 4; i++) grid.items[i] = new ItemStack("oak_planks", 1);
            var r = Recipes.FindCrafting(grid, null);
            Assert.IsNotNull(r);
            Assert.AreEqual("crafting_table", r.Assemble(grid).item.id);

            var big = new CraftingGrid(3, 3);
            for (int i = 0; i < 9; i++) if (i != 4) big.items[i] = new ItemStack("cobblestone", 1);
            var f = Recipes.FindCrafting(big, null);
            Assert.IsNotNull(f);
            Assert.AreEqual("furnace", f.Assemble(big).item.id);

            Assert.IsNotNull(Recipes.FindSmelting(Items.Get("raw_iron"), "furnace"), "raw iron smelts");
        }

        [Test]
        public void ItemStacksSurviveSerialization()
        {
            var s = new ItemStack("diamond_sword", 1);
            s.damage = 17;
            var back = ItemStack.Deserialize(s.Serialize());
            Assert.AreEqual("diamond_sword", back.item.id);
            Assert.AreEqual(17, back.damage);
        }

        [Test]
        public void NetherAndEndStructuresBelongToTheirDimensions()
        {
            // regression: fortresses and bastions once defaulted to the Overworld and never generated
            var fortress = StructureManager.Types.Find(t => t.id == "fortress");
            var bastion = StructureManager.Types.Find(t => t.id == "bastion_remnant");
            var city = StructureManager.Types.Find(t => t.id == "end_city");
            Assert.IsNotNull(fortress); Assert.IsNotNull(bastion); Assert.IsNotNull(city);
            Assert.AreEqual(DimensionId.Nether, fortress.dim);
            Assert.AreEqual(DimensionId.Nether, bastion.dim);
            Assert.AreEqual(DimensionId.End, city.dim);
            Assert.AreEqual(DimensionId.Overworld, StructureManager.Types.Find(t => t.id == "stronghold").dim);
        }

        [Test]
        public void NetherComplexesAlternateBetweenFortressAndBastion()
        {
            int fortresses = 0, bastions = 0;
            for (int rx = -20; rx < 20; rx++)
                if (NetherComplex.IsFortress(12345, rx * NetherComplex.Spacing * 16 + 8, 8)) fortresses++; else bastions++;
            Assert.Greater(fortresses, 5);
            Assert.Greater(bastions, 5);
        }

        [Test]
        public void SeedTextIsStable()
        {
            // numbers are taken as they are, text is hashed like Java's String.hashCode
            Assert.AreEqual(12345, CreateWorldScreen.ParseSeed("12345"));
            Assert.AreEqual(-7, CreateWorldScreen.ParseSeed(" -7 "));
            Assert.AreEqual(96354, CreateWorldScreen.ParseSeed("abc"));
        }

        [Test]
        public void EveryModelDefinitionHasBones()
        {
            int n = 0;
            foreach (var def in MobModels.All)
            {
                Assert.Greater(def.bones.Count, 0, def.name);
                n++;
            }
            Assert.GreaterOrEqual(n, 100, "model definitions");
            Assert.IsNotNull(MobModels.Get("ender_dragon"));
            Assert.IsNotNull(MobModels.Get("wither"));
        }
    }
}
