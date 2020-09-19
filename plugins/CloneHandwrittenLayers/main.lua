
-- Register all Toolbar actions and intialize all UI stuff
function initUi()
  app.registerUi({["menu"] = "Clone Handwritten Layers to Next Page", ["callback"] = "clone", ["accelerator"]="<Control><Shift>c"});
end

function clone()
  app.sidebarAction("COPY");
  app.changeBackgroundPdfPageNr(1, true);
  app.uiAction({["action"]="ACTION_GOTO_NEXT"});
  app.uiAction({["action"]="ACTION_DELETE_PAGE"});
  app.uiAction({["action"]="ACTION_GOTO_BACK"});
end