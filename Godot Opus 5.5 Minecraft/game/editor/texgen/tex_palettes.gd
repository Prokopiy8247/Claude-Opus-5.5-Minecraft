class_name TexPalettes
extends RefCounted
## Original colour palettes for the procedural texture generator.

const DYE := {
	"white": "#e9ecec", "orange": "#f07613", "magenta": "#bd44b3", "light_blue": "#3aafd9",
	"yellow": "#f8c627", "lime": "#70b919", "pink": "#ed8dac", "gray": "#3e4447",
	"light_gray": "#8e8e86", "cyan": "#158991", "purple": "#792aac", "blue": "#35399d",
	"brown": "#724728", "green": "#546d1b", "red": "#a12722", "black": "#141519",
}
const CONCRETE := {
	"white": "#cfd5d6", "orange": "#e06101", "magenta": "#a9309f", "light_blue": "#2489c7",
	"yellow": "#f1af15", "lime": "#5ea918", "pink": "#d5658e", "gray": "#373a3e",
	"light_gray": "#7d7d73", "cyan": "#157788", "purple": "#64209c", "blue": "#2c2e8f",
	"brown": "#603c20", "green": "#495b24", "red": "#8e2121", "black": "#080a0f",
}
const TERRACOTTA := {
	"white": "#d1b2a1", "orange": "#a15325", "magenta": "#95576c", "light_blue": "#706c8a",
	"yellow": "#ba8523", "lime": "#677535", "pink": "#a04d4e", "gray": "#392a23",
	"light_gray": "#876b62", "cyan": "#575b5b", "purple": "#764656", "blue": "#4a3b5b",
	"brown": "#4d3323", "green": "#4c532a", "red": "#8f3d2e", "black": "#251610",
}

# bark (4), log-top rings (3), planks (4), leaves colour (for untinted leaves)
const WOOD := {
	"oak": {bark = ["#6d5534", "#584429", "#7c623c", "#463620"], ring = ["#b4915c", "#9c7b4b", "#c6a068"],
		plank = ["#b99462", "#a7834f", "#c7a26b", "#8e6f41"]},
	"spruce": {bark = ["#3e2c17", "#312211", "#4b371e", "#271a0c"], ring = ["#7a5a34", "#694c2a", "#86663c"],
		plank = ["#76572f", "#664a28", "#83623a", "#563d20"]},
	"birch": {bark = ["#dcdbd4", "#c9c8c0", "#eeeee6", "#3c3632"], ring = ["#c7b77f", "#b4a36b", "#d6c78f"],
		plank = ["#c7b57b", "#b5a369", "#d6c68e", "#a08f58"]},
	"jungle": {bark = ["#59491d", "#473a16", "#6b5a25", "#3a2f10"], ring = ["#a67b4e", "#946b40", "#b78959"],
		plank = ["#a4744b", "#94663f", "#b3835a", "#7c5533"]},
	"acacia": {bark = ["#696259", "#57514a", "#7a736a", "#46413b"], ring = ["#b36236", "#9d552d", "#c47143"],
		plank = ["#ab5d34", "#9a522c", "#bb6c40", "#854523"]},
	"dark_oak": {bark = ["#3e2f1b", "#312514", "#4b3a23", "#241a0d"], ring = ["#4f3520", "#43301c", "#5c3f26"],
		plank = ["#462d16", "#3b2512", "#52361c", "#2e1d0c"]},
	"mangrove": {bark = ["#574539", "#473730", "#665243", "#3b2c24"], ring = ["#7b3a34", "#6a302b", "#8a443d"],
		plank = ["#763733", "#662e2b", "#85423d", "#562321"]},
	"cherry": {bark = ["#3c2027", "#2f171d", "#4b2a31", "#241116"], ring = ["#e1b5ad", "#d3a098", "#ecc6bf"],
		plank = ["#e3b4ac", "#d6a097", "#eec6bf", "#c48c84"]},
	"pale_oak": {bark = ["#5e5952", "#4f4a44", "#6c675f", "#403c37"], ring = ["#e6ddd6", "#d7ccc4", "#f0e9e3"],
		plank = ["#e5ddd6", "#d6cbc3", "#efe8e2", "#c5b8af"]},
	"crimson": {bark = ["#5c1b1f", "#4a1417", "#962f55", "#b8406e"], ring = ["#7a3a4f", "#693043", "#8c4a5f"],
		plank = ["#6b344b", "#5c2b40", "#7a3f58", "#4c2234"]},
	"warped": {bark = ["#3a3b4e", "#2d2e3e", "#1aa39b", "#29d0bf"], ring = ["#3a8e8c", "#2f7a78", "#48a19d"],
		plank = ["#2c6b64", "#245b55", "#347b73", "#1c4a45"]},
	"bamboo": {bark = ["#8b8d2c", "#767924", "#a0a236", "#5f611b"], ring = ["#c9bc5c", "#b7aa4c", "#d8cb6b"],
		plank = ["#d0c267", "#bfb057", "#dfd177", "#a89a44"]},
}

const ORE := {
	"coal": ["#2f2f2f", "#454545", "#1e1e1e"],
	"iron": ["#d8b294", "#c69575", "#e9cdb4"],
	"copper": ["#e0804f", "#a95d39", "#5fae8a", "#f09c6c"],
	"gold": ["#fcee4b", "#e2b41f", "#fffbb0"],
	"redstone": ["#ff1a1a", "#aa0000", "#ff7a7a"],
	"emerald": ["#17dd62", "#00a12a", "#8dffbc"],
	"lapis": ["#1e4ab0", "#153785", "#4f78d8"],
	"diamond": ["#5decf5", "#28bcc0", "#dcfeff"],
	"quartz": ["#ece6df", "#d3c8bd", "#ffffff"],
	"nether_gold": ["#fcee4b", "#e2b41f", "#fff59a"],
}

static func c(s: String) -> Color:
	return Color.html(s)


static func cols(a: Array) -> Array:
	var out := []
	for s in a:
		out.append(Color.html(s))
	return out
