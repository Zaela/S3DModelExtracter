
local lfs = require "lfs"
local eqg = require "luaeqg"

local list = iup.list{visiblelines = 16, expand = "VERTICAL", visiblecolumns = 14, sort = "YES"}

local filter = iup.text{visiblecolumns = 13, value = ""}

function filter:valuechanged_cb()
	local path = search_path
	if path then
		UpdateDirList(path)
	end
end

function UpdateDirList(path)
	list[1] = nil
	list.autoredraw = "NO"
	local i = 1
	local f = filter.value
	if f:len() > 0 then
		--make sure last char isn't a dangling % (will throw incomplete pattern error)
		if f:find("%%", -1) then
			f = f .. "%"
		end
		for str in lfs.dir(path) do
			if str:find(f) and str:find("_chr%.s3d$") then
				list[i] = str
				i = i + 1
			end
		end
	else
		for str in lfs.dir(path) do
			if str:find("_chr%.s3d$") then
				list[i] = str
				i = i + 1
			end
		end
	end
	list.autoredraw = "YES"
end

function list:action(str, pos, state)
	if state == 1 then
		local path = search_path .."\\".. str
		UpdateModelList(path)
	end
end

return iup.vbox{iup.hbox{iup.label{title = "Filter"}, filter; alignment = "ACENTER", gap = 5}, list;
	alignment = "ACENTER", gap = 5}
