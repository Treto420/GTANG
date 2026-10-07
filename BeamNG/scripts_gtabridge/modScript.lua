-- auto-loads the bridge extension when the mod is active
if extensions and extensions.load then extensions.load('gtaBridge') end
if setExtensionUnloadMode then setExtensionUnloadMode('gtaBridge', 'manual') end
