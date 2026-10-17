"""v4.16 (perf) : agrege les mesures de performance du test du jeu (-BRAutoTest -BRAutoTestPerf) et compare deux etiquettes.

Chaque execution du test ecrit Saved/AutoTest/Perf.csv (une ligne par scene x profil) et PerfPasses.csv (postes et passes
du processeur graphique, releves ProfileGPU) ; Tools/Tests/run_perf.ps1 archive chaque execution sous
Saved/PerfRuns/<etiquette>/run<n>_graine<s>_profil<p>/. Ce script lit ces dossiers (recursivement) et produit un tableau
markdown par scene x profil :
  - temps d'image : mediane des medianes, ecart des medianes (min..max, en %), mediane des 95 %, pire 99 %, longues images
    (> 33,3 / 50 / 100 ms : total et pour 1000 images), images/s, medianes du processeur graphique et des threads du jeu,
    du rendu et du RHI, memoire video ;
  - streaming : chunks construits et demontes (ms), composants ajoutes par seconde, chargements synchrones, PSO crees a la
    demande et creations trop longues ;
  - scanner (Level 6) : rayons, instances, inscriptions et materiaux par seconde, temps du scanner (traces, envoi) ;
  - postes du processeur graphique (mediane des executions de la mediane des releves) : file graphique, puis files de
    calcul asynchrone a part (elles se recouvrent avec la file graphique).
Avec --before / --after : les memes tableaux avant / apres et les ecarts (ms et %), threads du jeu et du rendu compris, et
un avertissement si la carte, le pilote, le processeur ou les resolutions different (un gain mesure sur une machine ne
vaut pas pour une autre).

  python Tools/Tests/perf_summary.py Saved/PerfRuns/base                        resume d'une etiquette
  python Tools/Tests/perf_summary.py --before Saved/PerfRuns/base --after Saved/PerfRuns/opt1 --out comparaison.md
  options : --by-seed (une ligne par graine du niveau), --include-unstable (les mesures dont le prechauffage n'a pas ete
            stable sont exclues par defaut et comptees a part)
Code de sortie : 0 ; 2 si aucune mesure n'est trouvee.
"""
import argparse
import csv
import os
import statistics
import sys

# Colonnes lues dans Perf.csv (ecrites par Source/Backrooms/Private/BRAutoTestV416Perf.cpp ; le test
# Tools/Tests/test_perf_summary.py verifie qu'elles y sont toutes)
PERF_COLUMNS = [
    "format", "jeu", "tag", "execution", "scene", "niveau", "graine_niveau", "profil", "rt_actif", "lumen_materiel", "hit_lighting", "megalights",
    "fenetre_x", "fenetre_y", "interne_x", "interne_y", "gpu", "pilote", "cpu", "prechauffe_stable", "images", "img_s", "mediane_ms", "p95_ms",
    "p99_ms", "pire_ms", "longues_33", "longues_50", "longues_100", "jeu_med_ms", "rendu_med_ms", "rhi_med_ms", "gpu_med_ms", "vram_utilisee_mo",
    "vram_max_mo", "vram_budget_mo", "ram_mo", "chunks_construits", "construction_ms", "chunks_demontes", "demontage_ms", "composants_par_s",
    "chargements_sync", "lidar_rayons_s", "lidar_instances_max", "lidar_inscriptions_s", "lidar_materiaux_s", "lidar_cpu_ms", "gpu_image_ms",
    "chargement_s", "prechauffe_s", "images_shaders",
    # v4.16 (perf, relecture)
    "pso_crees", "pso_saccades", "gpu_autres_files_ms", "lidar_traces_ms", "lidar_envoi_ms",
]
PASS_COLUMNS = ["format", "tag", "execution", "scene", "niveau", "graine_niveau", "profil", "type", "nom", "profondeur", "ms_med", "releves"]
CATEGORIES = ["megalights", "lumen", "scene_rt", "ombres", "brouillard", "translucidite_eau", "post", "base", "eclairage", "effets_ecran", "scene_gpu",
              "interface", "autre"]
SCENE_ORDER = ["fosses", "bassins", "marche0", "marche1", "lidar6"]
PROFILE_ORDER = ["Performance", "Qualite", "RTXFluide", "Cinematique"]


def num(value):
    """Nombre d'une cellule (None si vide ou illisible)"""
    if value is None:
        return None
    text = str(value).strip().replace(",", ".")
    if not text:
        return None
    try:
        return float(text)
    except ValueError:
        return None


def read_csv(path):
    with open(path, encoding="utf-8-sig", newline="") as handle:
        # Separateur ';' sans guillemets (le jeu remplace ';' et les fins de ligne dans les cellules) : un nom de passe qui
        # contient un guillemet reste tel quel
        return list(csv.DictReader(handle, delimiter=";", quoting=csv.QUOTE_NONE))


def find_files(roots, name):
    found = []
    for root in roots:
        if os.path.isfile(root):
            if os.path.basename(root) == name:
                found.append(root)
            elif name == "PerfPasses.csv" and os.path.basename(root) == "Perf.csv":
                sibling = os.path.join(os.path.dirname(root), name)
                if os.path.isfile(sibling):
                    found.append(sibling)
            continue
        for base, _dirs, files in os.walk(root):
            if name in files:
                found.append(os.path.join(base, name))
    return sorted(set(found))


def load(roots):
    """(lignes de Perf.csv, lignes de PerfPasses.csv), chaque ligne avec la cle 'source' (dossier de l'execution)"""
    rows = []
    for path in find_files(roots, "Perf.csv"):
        for row in read_csv(path):
            row["source"] = os.path.dirname(path)
            rows.append(row)
    passes = []
    for path in find_files(roots, "PerfPasses.csv"):
        for row in read_csv(path):
            row["source"] = os.path.dirname(path)
            passes.append(row)
    return rows, passes


def group_key(row, by_seed):
    return (row.get("scene", ""), row.get("profil", ""), row.get("graine_niveau", "") if by_seed else "")


def sort_key(key):
    scene, profile, seed = key
    s = SCENE_ORDER.index(scene) if scene in SCENE_ORDER else len(SCENE_ORDER)
    p = PROFILE_ORDER.index(profile) if profile in PROFILE_ORDER else len(PROFILE_ORDER)
    return (s, scene, p, profile, seed)


def median(values):
    values = [v for v in values if v is not None]
    return statistics.median(values) if values else None


def aggregate(rows, passes, by_seed=False, include_unstable=False):
    """{cle: statistiques} ; cle = (scene, profil, graine du niveau ou '')"""
    groups = {}
    for row in rows:
        groups.setdefault(group_key(row, by_seed), []).append(row)
    pass_groups = {}
    async_groups = {}
    for row in passes:
        kind = row.get("type")
        if kind not in ("poste", "poste_async"):
            continue
        key = group_key(row, by_seed)
        target = pass_groups if kind == "poste" else async_groups
        target.setdefault(key, {}).setdefault(row.get("nom", ""), {})[(row.get("source"), row.get("execution"))] = num(row.get("ms_med"))
    out = {}
    for key, members in groups.items():
        unstable = [r for r in members if num(r.get("prechauffe_stable")) == 0]
        used = members if include_unstable else [r for r in members if num(r.get("prechauffe_stable")) != 0]
        stats = {"n": len(used), "n_unstable": len(unstable), "n_all": len(members)}
        meds = [num(r.get("mediane_ms")) for r in used]
        meds = [m for m in meds if m is not None]
        stats["median_of_medians"] = statistics.median(meds) if meds else None
        stats["median_min"] = min(meds) if meds else None
        stats["median_max"] = max(meds) if meds else None
        stats["spread_pct"] = ((max(meds) - min(meds)) / statistics.median(meds) * 100.0) if meds and statistics.median(meds) > 0 else None
        stats["p95_median"] = median([num(r.get("p95_ms")) for r in used])
        p99 = [num(r.get("p99_ms")) for r in used]
        p99 = [v for v in p99 if v is not None]
        stats["p99_worst"] = max(p99) if p99 else None
        stats["p99_median"] = statistics.median(p99) if p99 else None
        worst = [num(r.get("pire_ms")) for r in used]
        worst = [v for v in worst if v is not None]
        stats["worst"] = max(worst) if worst else None
        frames = sum(int(num(r.get("images")) or 0) for r in used)
        stats["frames"] = frames
        for col in ("longues_33", "longues_50", "longues_100"):
            total = sum(int(num(r.get(col)) or 0) for r in used)
            stats[col] = total
            stats[col + "_per_1000"] = (total * 1000.0 / frames) if frames else None
        for col in ("img_s", "gpu_med_ms", "jeu_med_ms", "rendu_med_ms", "rhi_med_ms", "vram_utilisee_mo", "vram_budget_mo", "ram_mo", "chunks_construits",
                    "construction_ms", "chunks_demontes", "demontage_ms", "composants_par_s", "lidar_rayons_s", "lidar_instances_max",
                    "lidar_inscriptions_s", "lidar_materiaux_s", "lidar_cpu_ms", "gpu_image_ms", "chargement_s", "prechauffe_s", "images_shaders",
                    "gpu_autres_files_ms", "lidar_traces_ms", "lidar_envoi_ms", "pso_crees"):
            stats[col] = median([num(r.get(col)) for r in used])
        vmax = [num(r.get("vram_max_mo")) for r in used]
        vmax = [v for v in vmax if v is not None]
        stats["vram_max_mo"] = max(vmax) if vmax else None
        stats["chargements_sync_total"] = sum(int(num(r.get("chargements_sync")) or 0) for r in used)
        stats["pso_saccades_total"] = sum(int(num(r.get("pso_saccades")) or 0) for r in used)
        used_runs = {(r.get("source"), r.get("execution")) for r in used}
        cats = {}
        for name, values in pass_groups.get(key, {}).items():
            cats[name] = median([v for run, v in values.items() if run in used_runs])
        stats["categories"] = cats
        async_cats = {}
        for name, values in async_groups.get(key, {}).items():
            async_cats[name] = median([v for run, v in values.items() if run in used_runs])
        stats["async_categories"] = async_cats
        stats["machines"] = sorted({(r.get("gpu", ""), r.get("pilote", ""), r.get("cpu", "")) for r in members})
        stats["resolutions"] = sorted({"%sx%s -> %sx%s" % (r.get("interne_x", "?"), r.get("interne_y", "?"), r.get("fenetre_x", "?"), r.get("fenetre_y", "?"))
                                       for r in members})
        stats["render"] = sorted({"RT %s, Lumen %s, hit %s, MegaLights %s" % (r.get("rt_actif", "?"), "materiel" if num(r.get("lumen_materiel")) else "logiciel",
                                                                             r.get("hit_lighting", "?"), r.get("megalights", "?")) for r in members})
        stats["versions"] = sorted({r.get("jeu", "") for r in members})
        out[key] = stats
    return out


def fmt(value, digits=2, suffix=""):
    if value is None:
        return "-"
    return ("%." + str(digits) + "f%s") % (value, suffix)


def fmt_delta(before, after, digits=2):
    if before is None or after is None:
        return "-"
    delta = after - before
    pct = (delta / before * 100.0) if before else None
    sign = "+" if delta >= 0 else ""
    return "%s%.*f (%s)" % (sign, digits, delta, ("%s%.1f %%" % ("+" if pct >= 0 else "", pct)) if pct is not None else "-")


def label(key):
    scene, profile, seed = key
    return (scene, profile + ((" / " + seed) if seed else ""))


def context_lines(name, stats):
    machines = sorted({m for s in stats.values() for m in s["machines"]})
    resolutions = sorted({r for s in stats.values() for r in s["resolutions"]})
    versions = sorted({v for s in stats.values() for v in s["versions"]})
    runs = sum(s["n_all"] for s in stats.values())
    lines = ["**%s** : %d mesure(s), version(s) %s" % (name, runs, ", ".join(versions) or "-")]
    for gpu, driver, cpu in machines:
        lines.append("- machine : %s, pilote %s ; %s" % (gpu or "?", driver or "?", cpu or "?"))
    lines.append("- resolutions (interne -> fenetre) : %s" % ", ".join(resolutions))
    return lines, machines, resolutions


def frame_table(stats):
    lines = ["| Scene | Profil | n (instables) | Mediane (ms) | Ecart des medianes | 95 % (ms) | Pire 99 % (ms) | > 33 / 50 / 100 ms (pour 1000 img) | img/s | GPU (ms) | Jeu / rendu / RHI (ms) | VRAM (Mo, pic) |",
             "|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for key in sorted(stats, key=sort_key):
        s = stats[key]
        scene, profile = label(key)
        spread = "%s..%s (%s)" % (fmt(s["median_min"]), fmt(s["median_max"]), fmt(s["spread_pct"], 1, " %")) if s["median_min"] is not None else "-"
        longs = "%d / %d / %d (%s / %s / %s)" % (s["longues_33"], s["longues_50"], s["longues_100"], fmt(s["longues_33_per_1000"], 1),
                                                  fmt(s["longues_50_per_1000"], 1), fmt(s["longues_100_per_1000"], 1))
        lines.append("| %s | %s | %d (%d) | %s | %s | %s | %s | %s | %s | %s | %s / %s / %s | %s (%s) |" % (
            scene, profile, s["n"], s["n_unstable"], fmt(s["median_of_medians"]), spread, fmt(s["p95_median"]), fmt(s["p99_worst"]), longs, fmt(s["img_s"], 1),
            fmt(s["gpu_med_ms"]), fmt(s["jeu_med_ms"]), fmt(s["rendu_med_ms"]), fmt(s["rhi_med_ms"]), fmt(s["vram_utilisee_mo"], 0), fmt(s["vram_max_mo"], 0)))
    return lines


def streaming_table(stats):
    lines = ["| Scene | Profil | Chunks construits | Construction (ms) | Chunks demontes | Demontage (ms) | Composants/s | Chargements synchrones (total) | PSO crees (saccades, total) | Chargement (s) | Prechauffage (s) |",
             "|---|---|---|---|---|---|---|---|---|---|---|"]
    for key in sorted(stats, key=sort_key):
        s = stats[key]
        scene, profile = label(key)
        lines.append("| %s | %s | %s | %s | %s | %s | %s | %d | %s (%d) | %s | %s |" % (
            scene, profile, fmt(s["chunks_construits"], 0), fmt(s["construction_ms"], 1), fmt(s["chunks_demontes"], 0), fmt(s["demontage_ms"], 1),
            fmt(s["composants_par_s"], 1), s["chargements_sync_total"], fmt(s["pso_crees"], 0), s["pso_saccades_total"], fmt(s["chargement_s"], 1),
            fmt(s["prechauffe_s"], 1)))
    return lines


def lidar_table(stats):
    keys = [k for k in sorted(stats, key=sort_key) if stats[k]["lidar_rayons_s"] is not None]
    if not keys:
        return []
    lines = ["| Scene | Profil | Rayons/s | Instances (pic) | Inscriptions/s | Materiaux/s | Scanner (ms/image) | Traces (ms) | Envoi (ms) |", "|---|---|---|---|---|---|---|---|---|"]
    for key in keys:
        s = stats[key]
        scene, profile = label(key)
        lines.append("| %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (scene, profile, fmt(s["lidar_rayons_s"], 0), fmt(s["lidar_instances_max"], 0),
                                                                       fmt(s["lidar_inscriptions_s"], 0), fmt(s["lidar_materiaux_s"], 1), fmt(s["lidar_cpu_ms"], 3),
                                                                       fmt(s["lidar_traces_ms"], 3), fmt(s["lidar_envoi_ms"], 3)))
    return lines


def gpu_table(stats, field="categories", with_frame=True):
    keys = [k for k in sorted(stats, key=sort_key) if any(v for v in stats[k][field].values())]
    if not keys:
        return []
    head = "| Scene | Profil | " + ("Image (ms) | " if with_frame else "Total (ms) | ")
    lines = [head + " | ".join(CATEGORIES) + " |", "|---|---|---|" + "---|" * len(CATEGORIES)]
    for key in keys:
        s = stats[key]
        scene, profile = label(key)
        first = s["gpu_image_ms"] if with_frame else s["gpu_autres_files_ms"]
        lines.append("| %s | %s | %s | %s |" % (scene, profile, fmt(first), " | ".join(fmt(s[field].get(c)) for c in CATEGORIES)))
    return lines


def summary_markdown(name, stats):
    lines = ["# Mesures de performance : %s" % name, ""]
    ctx, _m, _r = context_lines(name, stats)
    lines += ctx + ["", "Les mesures dont le prechauffage n'a pas ete stable sont comptees entre parentheses et exclues des medianes.", "",
                    "## Temps d'image", ""]
    lines += frame_table(stats) + ["", "## Streaming et chargements", ""] + streaming_table(stats)
    lidar = lidar_table(stats)
    if lidar:
        lines += ["", "## Scanner LiDAR (Level 6)", ""] + lidar
    gpu = gpu_table(stats)
    if gpu:
        lines += ["", "## Postes du processeur graphique (ProfileGPU, file graphique, ms)", ""] + gpu
    gpu_async = gpu_table(stats, "async_categories", with_frame=False)
    if gpu_async:
        lines += ["", "## Files de calcul asynchrone (ProfileGPU, ms ; en parallele de la file graphique)", ""] + gpu_async
    return "\n".join(lines) + "\n"


def compare_markdown(before_name, before, after_name, after):
    lines = ["# Mesures de performance : %s -> %s" % (before_name, after_name), ""]
    ctx_b, machines_b, res_b = context_lines("avant (" + before_name + ")", before)
    ctx_a, machines_a, res_a = context_lines("apres (" + after_name + ")", after)
    lines += ctx_b + [""] + ctx_a + [""]
    warnings = []
    if machines_b != machines_a:
        warnings.append("ATTENTION : carte, pilote ou processeur differents entre avant et apres : les ecarts ne mesurent pas seulement le code.")
    if res_b != res_a:
        warnings.append("ATTENTION : resolutions differentes entre avant et apres.")
    only_b = sorted(set(before) - set(after), key=sort_key)
    only_a = sorted(set(after) - set(before), key=sort_key)
    if only_b:
        warnings.append("Seulement avant : %s" % ", ".join("%s %s" % label(k) for k in only_b))
    if only_a:
        warnings.append("Seulement apres : %s" % ", ".join("%s %s" % label(k) for k in only_a))
    lines += warnings + ([""] if warnings else [])
    lines += ["## Temps d'image (avant -> apres, ecart)", "",
              "| Scene | Profil | n avant / apres | Mediane (ms) | Ecart | 95 % (ms) | Pire 99 % (ms) | > 33 ms pour 1000 img | > 50 ms pour 1000 img | GPU (ms) | Jeu (ms) | Rendu (ms) | VRAM (Mo) |",
              "|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    common = sorted(set(before) & set(after), key=sort_key)
    for key in common:
        b, a = before[key], after[key]
        scene, profile = label(key)
        lines.append("| %s | %s | %d / %d | %s -> %s | %s | %s | %s | %s -> %s | %s -> %s | %s | %s | %s | %s |" % (
            scene, profile, b["n"], a["n"], fmt(b["median_of_medians"]), fmt(a["median_of_medians"]), fmt_delta(b["median_of_medians"], a["median_of_medians"]),
            fmt_delta(b["p95_median"], a["p95_median"]), fmt_delta(b["p99_worst"], a["p99_worst"]), fmt(b["longues_33_per_1000"], 1),
            fmt(a["longues_33_per_1000"], 1), fmt(b["longues_50_per_1000"], 1), fmt(a["longues_50_per_1000"], 1), fmt_delta(b["gpu_med_ms"], a["gpu_med_ms"]),
            fmt_delta(b["jeu_med_ms"], a["jeu_med_ms"]), fmt_delta(b["rendu_med_ms"], a["rendu_med_ms"]), fmt_delta(b["vram_utilisee_mo"], a["vram_utilisee_mo"], 0)))
    lines += ["", "## Streaming (avant -> apres)", "",
              "| Scene | Profil | Construction (ms) | Demontage (ms) | Composants/s | Chargements synchrones | Saccades de PSO |", "|---|---|---|---|---|---|---|"]
    for key in common:
        b, a = before[key], after[key]
        scene, profile = label(key)
        lines.append("| %s | %s | %s | %s | %s | %d -> %d | %d -> %d |" % (scene, profile, fmt_delta(b["construction_ms"], a["construction_ms"], 1),
                                                                       fmt_delta(b["demontage_ms"], a["demontage_ms"], 1), fmt_delta(b["composants_par_s"], a["composants_par_s"], 1),
                                                                       b["chargements_sync_total"], a["chargements_sync_total"], b["pso_saccades_total"],
                                                                       a["pso_saccades_total"]))
    gpu_keys = [k for k in common if before[k]["categories"] and after[k]["categories"]]
    if gpu_keys:
        lines += ["", "## Postes du processeur graphique (ms, avant -> apres)", "", "| Scene | Profil | Image | " + " | ".join(CATEGORIES) + " |",
                  "|---|---|---|" + "---|" * len(CATEGORIES)]
        for key in gpu_keys:
            b, a = before[key], after[key]
            scene, profile = label(key)
            cells = [fmt_delta(b["categories"].get(c), a["categories"].get(c)) for c in CATEGORIES]
            lines.append("| %s | %s | %s | %s |" % (scene, profile, fmt_delta(b["gpu_image_ms"], a["gpu_image_ms"]), " | ".join(cells)))
    return "\n".join(lines) + "\n", warnings


def main(argv=None):
    ap = argparse.ArgumentParser(description="Agrege les mesures de performance (Perf.csv, PerfPasses.csv) et compare deux etiquettes")
    ap.add_argument("paths", nargs="*", help="dossiers (parcourus recursivement) ou fichiers Perf.csv")
    ap.add_argument("--before", nargs="+", default=None, help="mesures d'avant (dossiers ou Perf.csv)")
    ap.add_argument("--after", nargs="+", default=None, help="mesures d'apres (dossiers ou Perf.csv)")
    ap.add_argument("--by-seed", action="store_true", help="une ligne par graine du niveau")
    ap.add_argument("--include-unstable", action="store_true", help="compter aussi les mesures dont le prechauffage n'a pas ete stable")
    ap.add_argument("--out", default="", help="fichier markdown (sinon : sortie standard)")
    args = ap.parse_args(argv)
    if (args.before is None) != (args.after is None):
        ap.error("--before et --after vont ensemble")
    if args.before is not None:
        rows_b, passes_b = load(args.before)
        rows_a, passes_a = load(args.after)
        if not rows_b or not rows_a:
            print("Aucune mesure trouvee (%s : %d, %s : %d)" % (" ".join(args.before), len(rows_b), " ".join(args.after), len(rows_a)))
            return 2
        before = aggregate(rows_b, passes_b, args.by_seed, args.include_unstable)
        after = aggregate(rows_a, passes_a, args.by_seed, args.include_unstable)
        text, _warnings = compare_markdown(" ".join(os.path.basename(os.path.normpath(p)) for p in args.before), before,
                                           " ".join(os.path.basename(os.path.normpath(p)) for p in args.after), after)
    else:
        if not args.paths:
            ap.error("donner au moins un dossier, ou --before et --after")
        rows, passes = load(args.paths)
        if not rows:
            print("Aucune mesure trouvee dans %s" % " ".join(args.paths))
            return 2
        text = summary_markdown(" ".join(os.path.basename(os.path.normpath(p)) for p in args.paths), aggregate(rows, passes, args.by_seed, args.include_unstable))
    if args.out:
        with open(args.out, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
        print("Ecrit : %s" % args.out)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
