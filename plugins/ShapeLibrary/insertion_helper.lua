local sep = package.config:sub(1, 1) -- path separator depends on OS
local sourcePath = debug.getinfo(1).source:match("@?(.*" .. sep .. ")")
local stroke_io = require "stroke_io"

local _M = {}
-- Function to insert strokes for a shape (need to extract the shape name from the dictionary)
function _M.insert_stroke(shape_name)
    local filepath = sourcePath .. "Shapes" .. sep .. shape_name .. ".lua"
    local strokes = stroke_io.read_strokes_from_file(filepath)
    if strokes and #strokes > 0 then
        local refs = app.addStrokes({ strokes = strokes , allowUndoRedoAction = "grouped" })
        if app.addToSelection then
            app.addToSelection(refs)
        else
            print("Cannot add shape to selection because of missing API, consider upgrading Xournal++")
        end
        app.refreshPage()
    end
end

return _M
