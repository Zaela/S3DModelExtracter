
local eqg = require "luaeqg"

local list = iup.list{visiblelines = 10, expand = "VERTICAL", visiblecolumns = 12}

local wld_list, wld_ent, by_name, cur, cur_name

local ipairs = ipairs
local pairs = pairs
local pcall = pcall

local function ListModels(wld_list)
	list[1] = nil
	list.autoredraw = "NO"
	for i, frag in ipairs(wld_list) do
		local name = frag:GetName()
		list[i] = name
		by_name[name] = frag
	end
	list.autoredraw = "YES"
end

local function FindWLD(dir)
	for i, ent in ipairs(dir) do
		if ent.name:find("%.wld$") then
			local s, err = pcall(eqg.OpenEntry, ent)
			if s then
				s, err = pcall(wld.Read, ent)
				if s then
					wld_ent = ent
					wld_list = err
					ListModels(err)
					return
				end
			end
			return error_popup(err)
		end
	end
end

function UpdateModelList(path)
	if wld_list then
		wld.Close(wld_list)
	end
	wld_list = nil
	wld_ent = nil
	cur = nil
	eqg.CloseDirectory(open_dir)
	open_path = path
	local s, dir = pcall(eqg.LoadDirectory, path)
	if not s then
		error_popup(dir)
		return
	end
	open_dir = dir
	by_name = {}
	FindWLD(dir)
end

function list:action(str, pos, state)
	if state == 1 and wld_list then
		local frag = by_name[str]
		if frag then
			cur = frag
			cur_name = str
		end
	end
end

local function Extract()
	if not wld_ent or not cur then return end
	--need new wld ent, list of images to export
	local s, image_names, ptr, len = pcall(wld.Extract, wld_ent, wld_list.string_block, cur, cur_name:sub(1, 3))
	if s then
		local name
		local input = iup.text{visiblecolumns = 16}
		local getid
		local but = iup.button{title = "Done", action = function() name = input.value; getid:hide() end}
		getid = iup.dialog{iup.vbox{
			iup.label{title = "Please enter a name for the new _chr.s3d file."},
			input, but, gap = 12, nmargin = "15x15", alignment = "ACENTER"};
			k_any = function(self, key) if key == iup.K_CR then but:action() end end}
		iup.Popup(getid)
		iup.Destroy(getid)
		if name then
			name = name:match("[%w_]+")
			if name then
				local wld_name = name .. "_chr.wld"
				local wld_file = {
					ptr = ptr,
					inflated_len = len,
					name = wld_name,
					crc = eqg.CalcCRC(wld_name),
					decompressed = true,
				}
				local dir = {wld_file}
				local textures = {}
				for i, name in ipairs(image_names) do
					textures[name:lower()] = true
				end
				for i, ent in ipairs(open_dir) do
					if textures[ent.name] then
						table.insert(dir, ent)
					end
				end
				local err
				name = name .. "_chr.s3d"
				s, err = pcall(eqg.WriteDirectory, name, dir)
				if s then
					local msg = iup.messagedlg{title = "Success", value = "Successfully extracted model ".. cur_name .." to ".. name .."."}
					iup.Popup(msg)
					iup.Destroy(msg)
				else
					error_popup(err)
				end
			end
		end
	else
		error_popup(image_names)
	end
end

local function Rename()
	if not wld_ent or not cur then return end
	local from = cur_name:sub(1, 3)
	local name
	local input = iup.text{visiblecolumns = 3, value = from, mask = "/l/l/l"}
	local getid
	local but = iup.button{title = "Done", action = function() name = input.value; getid:hide() end}
	getid = iup.dialog{iup.vbox{
		iup.label{title = "Enter a new 3-letter name for this model:"},
		input, but, gap = 12, nmargin = "15x15", alignment = "ACENTER"};
		k_any = function(self, key) if key == iup.K_CR then but:action() end end}
	iup.Popup(getid)
	iup.Destroy(getid)
	if name and name:len() >= 3 then
		name = name:upper()
		local s, err = pcall(wld.Rename, wld_ent, from, name)
		if s then
			local wld_name = from:lower() .. "_chr.wld"
			for i, ent in ipairs(open_dir) do
				if ent.name == wld_name then
					local new_name = name:sub(1, 3):lower() .. "_chr.wld"
					ent.name = new_name
					ent.crc = eqg.CalcCRC(new_name)
					break
				end
			end
			s, err = pcall(eqg.WriteDirectory, open_path, open_dir)
			if s then
				UpdateModelList(open_path)
				return
			end
		end
		error_popup(err)
	end
end

function list:button_cb(button, pressed, x, y)
	if button == iup.BUTTON3 and pressed == 0 then
		local mx, my = iup.GetGlobal("CURSORPOS"):match("(%d+)x(%d+)")
		local active = cur and "YES" or "NO"
		local menu = iup.menu{
			iup.item{title = "Extract Model", action = Extract, active = active},
			iup.separator{},
			iup.item{title = "Rename Model", action = Rename, active = active},
		}
		iup.Popup(menu, mx, my)
		iup.Destroy(menu)
	end
end

return iup.vbox{list; alignment = "ACENTER", gap = 5}
