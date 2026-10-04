"""
Execute automatiquement par Unreal (plugin Python) au demarrage de l'editeur.
Si les ressources du jeu n'ont pas encore ete importees, lance backrooms_setup.run()
une fois l'editeur completement charge.

Le plugin Python est aussi charge quand le jeu tourne sans editeur depuis le binaire de l'editeur
("Standalone Game", -game, serveur dedie) et pendant les commandlets (cuisson, import...). Les fonctions
d'edition y sont indisponibles (les appeler fait planter le jeu) : on ne fait alors rien.
"""
import unreal

_state = {"ticks": 0, "handle": None}


def _is_interactive_editor():
    try:
        if not unreal.is_editor():
            return False  # jeu autonome lance depuis l'editeur (-game) : pas d'editeur
    except Exception:
        pass
    try:
        cmd = " " + unreal.SystemLibrary.get_command_line().lower()
        for flag in (" -game", " -server", " -run=", " -nullrhi"):
            if flag in cmd:
                return False
    except Exception:
        pass
    return True


def _on_tick(delta_seconds):
    _state["ticks"] += 1
    if _state["ticks"] < 90:
        return
    try:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        if registry.is_loading_assets():
            return
    except Exception:
        pass
    unreal.unregister_slate_post_tick_callback(_state["handle"])
    try:
        import backrooms_setup
        if backrooms_setup.needs_setup():
            backrooms_setup.run(force=False)
        else:
            unreal.log("[Backrooms] Ressources deja installees.")
    except Exception as e:
        unreal.log_error("[Backrooms] Echec de l'installation automatique : %s" % e)


if _is_interactive_editor():
    try:
        _state["handle"] = unreal.register_slate_post_tick_callback(_on_tick)
    except Exception as e:
        unreal.log_error("[Backrooms] init_unreal : %s" % e)
