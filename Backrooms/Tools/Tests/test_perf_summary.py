"""v4.16 (perf) : tests de Tools/Tests/perf_summary.py (agregation des mesures de performance, comparaison avant / apres).

  python -m unittest Tools/Tests/test_perf_summary.py        (depuis Backrooms/)

Des Perf.csv et PerfPasses.csv sont ecrits comme le test du jeu les ecrit (UTF-8 avec BOM, separateur ';', colonnes de
BRAutoTestV416Perf.cpp), dans des dossiers d'executions comme ceux de Tools/Tests/run_perf.ps1. On verifie aussi que
chaque colonne lue par le script est bien ecrite par le test du jeu, et que run_perf.ps1 passe les options que le test lit.
"""
import os
import re
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import perf_summary  # noqa: E402

PERF_CPP = os.path.join(ROOT, "Source", "Backrooms", "Private", "BRAutoTestV416Perf.cpp")
RUN_PERF = os.path.join(HERE, "run_perf.ps1")


def written_columns():
    """Colonnes de Perf.csv ecrites par le test du jeu (Row.Int / Num / Text / Empty et la boucle des colonnes du scanner)"""
    with open(PERF_CPP, encoding="utf-8") as handle:
        source = handle.read()
    names = re.findall(r'Row\.(?:Int|Num|Text|Empty)\(TEXT\("([a-z0-9_]+)"\)', source)
    names += re.findall(r'TEXT\("(lidar_[a-z_]+)"\)', source)
    names += ["gpu_%s_ms" % c for c in perf_summary.CATEGORIES] + ["gpu_image_ms", "gpu_racine_ms", "gpu_attente_ms", "gpu_autres_files_ms"]
    return set(names)


def perf_row(scene, profile, median, p95, p99, worst, longs=(0, 0, 0), stable=1, gpu="NVIDIA GeForce RTX 5050 Laptop GPU", driver="581.57",
             run=1, frames=1000, gpu_ms=9.0, vram=5000, built=0, extra=None):
    row = {c: "" for c in perf_summary.PERF_COLUMNS}
    row.update({
        "format": "1", "jeu": "v4.15", "tag": "t", "execution": str(run), "scene": scene, "niveau": "0", "graine_niveau": "1896875771", "profil": profile,
        "rt_actif": "1", "lumen_materiel": "1", "hit_lighting": "0", "megalights": "1", "fenetre_x": "1536", "fenetre_y": "960", "interne_x": "1229",
        "interne_y": "768", "gpu": gpu, "pilote": driver, "cpu": "13th Gen Intel(R) Core(TM) i7-13620H", "prechauffe_stable": str(stable),
        "images": str(frames), "img_s": "%.1f" % (1000.0 / median), "mediane_ms": "%.3f" % median, "p95_ms": "%.3f" % p95, "p99_ms": "%.3f" % p99,
        "pire_ms": "%.3f" % worst, "longues_33": str(longs[0]), "longues_50": str(longs[1]), "longues_100": str(longs[2]), "jeu_med_ms": "4.000",
        "rendu_med_ms": "5.000", "rhi_med_ms": "3.000", "gpu_med_ms": "%.3f" % gpu_ms, "vram_utilisee_mo": str(vram), "vram_max_mo": str(vram + 100),
        "vram_budget_mo": "7600", "ram_mo": "3900", "chunks_construits": str(built), "construction_ms": "%.2f" % (built * 15.0), "chunks_demontes": "0",
        "demontage_ms": "0.00", "composants_par_s": "0.0", "chargements_sync": "0", "gpu_image_ms": "%.3f" % gpu_ms, "chargement_s": "9.5",
        "prechauffe_s": "8.4", "images_shaders": "0", "pso_crees": str(run * 2), "pso_saccades": str(run - 1), "gpu_autres_files_ms": "1.500",
    })
    if extra:
        row.update(extra)
    return row


def write_run(folder, rows, passes=()):
    os.makedirs(folder, exist_ok=True)
    columns = list(rows[0].keys())
    with open(os.path.join(folder, "Perf.csv"), "w", encoding="utf-8-sig", newline="") as handle:
        handle.write(";".join(columns) + "\r\n")
        for row in rows:
            handle.write(";".join(str(row[c]) for c in columns) + "\r\n")
    with open(os.path.join(folder, "PerfPasses.csv"), "w", encoding="utf-8-sig", newline="") as handle:
        handle.write("format;jeu;tag;execution;scene;niveau;graine_niveau;profil;type;nom;profondeur;ms_med;ms_min;ms_max;releves;gpu_image_ms_med\r\n")
        for scene, profile, run, kind, name, ms in passes:
            handle.write("1;v4.15;t;%d;%s;0;1896875771;%s;%s;%s;-1;%.3f;%.3f;%.3f;3;9.000\r\n" % (run, scene, profile, kind, name, ms, ms, ms))


class PerfSummaryTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="perf_summary_")

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def make_tag(self, name, medians, unstable_run=None, gpu="NVIDIA GeForce RTX 5050 Laptop GPU", lumen=(4.0, 4.4, 4.2)):
        for index, med in enumerate(medians):
            run = index + 1
            rows = [perf_row("fosses", "Cinematique", med, med * 1.1, med * 1.2 + run, med * 2, longs=(run, 1, 0), run=run,
                             stable=0 if run == unstable_run else 1, gpu=gpu, gpu_ms=med - 1.0),
                    perf_row("marche1", "Qualite", 10.0, 12.0, 20.0, 40.0, run=run, built=4)]
            passes = [("fosses", "Cinematique", run, "poste", "lumen", lumen[index % len(lumen)]),
                      ("fosses", "Cinematique", run, "poste", "megalights", 20.0),
                      ("fosses", "Cinematique", run, "poste_async", "lumen", 1.0 + run),
                      ("fosses", "Cinematique", run, "poste_async", "megalights", 0.0),
                      ("fosses", "Cinematique", run, "passe", "<root>/Scene", 40.0)]
            write_run(os.path.join(self.tmp, name, "run%d_graine0_profilall" % run), rows, passes)
        return os.path.join(self.tmp, name)

    def test_columns_written_by_the_game(self):
        written = written_columns()
        missing = [c for c in perf_summary.PERF_COLUMNS if c not in written]
        self.assertEqual(missing, [], "colonnes lues par perf_summary.py et absentes de BRAutoTestV416Perf.cpp : %s" % missing)

    def test_categories_match_the_game(self):
        with open(os.path.join(ROOT, "Source", "Backrooms", "Public", "BRPerfLogic.h"), encoding="utf-8") as handle:
            header = handle.read()
        block = re.search(r'static const char\* const Names\[NumCategories\] = \{([^}]*)\}', header)
        self.assertIsNotNone(block)
        names = re.findall(r'"([a-z_]+)"', block.group(1))
        self.assertEqual(sorted(names), sorted(perf_summary.CATEGORIES))

    def test_run_perf_passes_the_options_the_game_reads(self):
        with open(PERF_CPP, encoding="utf-8") as handle:
            source = handle.read()
        with open(RUN_PERF, encoding="utf-8-sig") as handle:
            script = handle.read()
        for option in ("BRAutoTestPerf", "BRPerfTag=", "BRPerfRun=", "BRPerfSeed=", "BRPerfScenes=", "BRPerfSeconds=", "BRPerfGpu=", "BRProfile="):
            self.assertIn(option, script, "run_perf.ps1 ne passe pas -%s" % option)
        for option in ("BRPerfTag=", "BRPerfRun=", "BRPerfSeed=", "BRPerfScenes=", "BRPerfSeconds=", "BRPerfGpu=", "BRProfile="):
            self.assertIn('TEXT("%s")' % option, source, "le test du jeu ne lit pas -%s" % option)

    def test_aggregate_one_tag(self):
        tag = self.make_tag("base", [45.0, 47.0, 46.0])
        rows, passes = perf_summary.load([tag])
        self.assertEqual(len(rows), 6)
        stats = perf_summary.aggregate(rows, passes)
        s = stats[("fosses", "Cinematique", "")]
        self.assertEqual(s["n"], 3)
        self.assertAlmostEqual(s["median_of_medians"], 46.0)
        self.assertAlmostEqual(s["median_min"], 45.0)
        self.assertAlmostEqual(s["median_max"], 47.0)
        self.assertAlmostEqual(s["spread_pct"], 2.0 / 46.0 * 100.0)
        # Pire 99 % : 47 x 1,2 + 2 (execution 2)
        self.assertAlmostEqual(s["p99_worst"], 47.0 * 1.2 + 2)
        self.assertEqual(s["longues_33"], 1 + 2 + 3)
        self.assertAlmostEqual(s["longues_33_per_1000"], 6 * 1000.0 / 3000)
        self.assertEqual(s["longues_50"], 3)
        self.assertAlmostEqual(s["categories"]["lumen"], 4.2)
        self.assertAlmostEqual(s["categories"]["megalights"], 20.0)
        # v4.16 (perf, relecture) : files asynchrones a part (jamais melangees aux postes de la file graphique)
        self.assertAlmostEqual(s["async_categories"]["lumen"], 3.0)
        self.assertNotIn("poste_async", s["categories"])
        self.assertAlmostEqual(s["categories"]["lumen"], 4.2)
        # PSO : mediane des creations (2, 4, 6), saccades additionnees (0 + 1 + 2)
        self.assertAlmostEqual(s["pso_crees"], 4.0)
        self.assertEqual(s["pso_saccades_total"], 3)
        self.assertAlmostEqual(stats[("marche1", "Qualite", "")]["chunks_construits"], 4)
        text = perf_summary.summary_markdown("base", stats)
        self.assertIn("| fosses | Cinematique | 3 (0) | 46.00 |", text)
        self.assertIn("Postes du processeur graphique", text)
        self.assertIn("Files de calcul asynchrone", text)
        self.assertIn("| 4 (3) |", text)
        self.assertIn("RTX 5050 Laptop", text)

    def test_unstable_runs_are_excluded_by_default(self):
        tag = self.make_tag("base", [45.0, 90.0, 46.0], unstable_run=2)
        rows, passes = perf_summary.load([tag])
        s = perf_summary.aggregate(rows, passes)[("fosses", "Cinematique", "")]
        self.assertEqual((s["n"], s["n_unstable"]), (2, 1))
        self.assertAlmostEqual(s["median_of_medians"], 45.5)
        self.assertAlmostEqual(s["categories"]["lumen"], (4.0 + 4.2) / 2)
        s_all = perf_summary.aggregate(rows, passes, include_unstable=True)[("fosses", "Cinematique", "")]
        self.assertEqual(s_all["n"], 3)
        self.assertAlmostEqual(s_all["median_of_medians"], 46.0)

    def test_compare_two_tags(self):
        before = self.make_tag("base", [45.0, 47.0, 46.0])
        after = self.make_tag("opt", [30.0, 31.0, 29.0], lumen=(3.0, 3.0, 3.0))
        stats_b = perf_summary.aggregate(*perf_summary.load([before]))
        stats_a = perf_summary.aggregate(*perf_summary.load([after]))
        text, warnings = perf_summary.compare_markdown("base", stats_b, "opt", stats_a)
        self.assertEqual(warnings, [])
        self.assertIn("46.00 -> 30.00", text)
        self.assertIn("-16.00 (-34.8 %)", text)
        self.assertIn("-1.20 (-28.6 %)", text)  # lumen 4,2 -> 3,0
        self.assertIn("| 3 -> 3 |", text)  # saccades de PSO
        self.assertIn("+0.00 (+0.0 %)", text)  # threads du jeu et du rendu (4 et 5 ms des deux cotes)

    def test_compare_warns_on_other_hardware(self):
        before = self.make_tag("base", [45.0, 47.0, 46.0])
        after = self.make_tag("autre", [30.0, 31.0, 29.0], gpu="NVIDIA GeForce RTX 4080")
        _text, warnings = perf_summary.compare_markdown("base", perf_summary.aggregate(*perf_summary.load([before])), "autre",
                                                        perf_summary.aggregate(*perf_summary.load([after])))
        self.assertTrue(any("carte" in w for w in warnings))

    def test_main_writes_markdown_and_reports_missing(self):
        tag = self.make_tag("base", [45.0])
        out = os.path.join(self.tmp, "resume.md")
        self.assertEqual(perf_summary.main([tag, "--out", out]), 0)
        with open(out, encoding="utf-8") as handle:
            self.assertIn("# Mesures de performance", handle.read())
        empty = os.path.join(self.tmp, "vide")
        os.makedirs(empty)
        self.assertEqual(perf_summary.main([empty]), 2)
        self.assertEqual(perf_summary.main(["--before", tag, "--after", tag, "--out", out]), 0)

    def test_empty_cells_are_ignored(self):
        self.assertIsNone(perf_summary.num(""))
        self.assertIsNone(perf_summary.num(None))
        self.assertEqual(perf_summary.num("1,5"), 1.5)
        self.assertEqual(perf_summary.fmt(None), "-")
        self.assertEqual(perf_summary.fmt_delta(None, 1.0), "-")


if __name__ == "__main__":
    unittest.main()
