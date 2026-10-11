import asyncio
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys
import unittest

from qiskit.providers.basic_provider import BasicSimulator
from manyspace.workflow import generate_quantum, observed_quantum, generate_on_backend, quantum_from_legacy

ROOT=Path(__file__).resolve().parents[1]
STUDIO=ROOT/"build/ManyspaceStudio.app/Contents/MacOS/ManyspaceStudio"


class QuantumWorkflow(unittest.TestCase):
    def test_reproducible_quantum_only_artifact(self):
        a=generate_quantum(16,9,3,.73)
        self.assertEqual(a,generate_quantum(16,9,3,.73))
        self.assertTrue(all("distance_ms" not in room for room in a["rooms"]))
        self.assertEqual(a["rooms"][0]["source"],0)
        b=generate_quantum(16,9,3,.23)
        self.assertNotEqual(a["transitions"],b["transitions"])
        self.assertEqual(a["baseline_kind"],"computed ideal dephased reference")
        legacy=json.loads((ROOT/"presets/reference64.json").read_text())
        q=quantum_from_legacy(legacy)
        for i,r in enumerate(legacy["rooms"]):
            for j,destination in enumerate(legacy["rooms"]):
                self.assertEqual(q["transitions"][r["source"]][destination["source"]],legacy["transitions"][i][j])

    def test_missing_source_and_bad_counts_rejected(self):
        q=generate_quantum(16)
        with self.assertRaisesRegex(ValueError,"no samples"):
            observed_quantum(q,{"0"*14:100},"test",100)
        with self.assertRaisesRegex(ValueError,"layout"):
            observed_quantum(q,{"not binary":100},"test",100)
        with self.assertRaisesRegex(ValueError,"shot total"):
            observed_quantum(q,{"0"*14:99},"test",100)
        counts={format(i,"014b"):2 for i in range(16)}
        counts["0"] = 1
        observed=observed_quantum(q,counts,"test",33)
        self.assertEqual(sum(observed["measurements"].values()),33)
        self.assertEqual(observed["measurements"]["0"*14],3)

    def test_actual_single_backend_submission(self):
        class Counted:
            name="BasicSimulator local test"
            calls=0
            def run(self,circuit,**kwargs):
                self.calls+=1
                return BasicSimulator().run(circuit,seed_simulator=23,**kwargs)
        backend=Counted()
        result=asyncio.run(generate_on_backend(backend,16,7,3,.73,4096))
        self.assertEqual(backend.calls,1)
        self.assertEqual(result["generation"]["shots"],4096)
        self.assertTrue(all(n>0 for n in result["generation"]["source_coverage"]))
        self.assertEqual(sum(result["measurements"].values()),4096)
        for row in result["transitions"]:
            self.assertAlmostEqual(sum(row),1)
        if shutil.which("node"):
            script="const fs=require('fs'),m=require('./web/model.js');m.validateQ(JSON.parse(fs.readFileSync(0,'utf8')));"
            subprocess.run(["node","-e",script],input=json.dumps(result),text=True,cwd=ROOT,check=True,capture_output=True)

    @unittest.skipUnless(sys.platform=="darwin" and STUDIO.exists() and shutil.which("node"),"make studio on Mac with Node installed")
    def test_character_fields_reach_native_and_reject_invalid_values(self):
        subprocess.run(["node","tests/test_workflow.js"],cwd=ROOT,check=True,capture_output=True)
        def smoke(path):
            return subprocess.run([str(STUDIO),"--scene-smoke",str(path)],capture_output=True,text=True)
        directory=ROOT/"build/workflow"
        baseline=json.loads(smoke(directory/"character-base.json").stdout)
        for path in directory.glob("character-*.json"):
            if path.stem=="character-base":
                continue
            with self.subTest(parameter=path.stem):
                result=smoke(path);self.assertEqual(result.returncode,0,result.stderr)
                output=json.loads(result.stdout)
                self.assertEqual(output["room"],baseline["room"])
                self.assertGreater(abs(output["energy"]-baseline["energy"]),1e-5)
                scene=json.loads(path.read_text())
                scene["rooms"][0][path.stem.removeprefix("character-")]=None
                invalid=directory/"invalid-character.json";invalid.write_text(json.dumps(scene))
                self.assertEqual(smoke(invalid).returncode,1)

    @unittest.skipUnless(sys.platform=="darwin" and STUDIO.exists() and shutil.which("node"),"make studio on Mac with Node installed")
    def test_scene_import_reaches_cpp_audio_and_rejects_bad_snapshot(self):
        subprocess.run(["node","tests/test_workflow.js"],cwd=ROOT,check=True,capture_output=True)
        outputs=[]
        for name in ("scene","edited","room-edited","room-filtered","room-stereo","migrated-v2"):
            result=subprocess.run([str(STUDIO),"--scene-smoke",str(ROOT/f"build/workflow/{name}.json")],capture_output=True,text=True,check=True)
            outputs.append(json.loads(result.stdout))
        self.assertTrue(all(r["rooms"]==64 and r["energy"]>0 for r in outputs))
        self.assertNotEqual(outputs[0]["energy"],outputs[1]["energy"])
        self.assertEqual(outputs[0]["room"],outputs[2]["room"])
        self.assertGreater(abs(outputs[0]["energy"]-outputs[2]["energy"]),.001,
                           "Changing only the selected room's speed must affect C++ output")
        self.assertEqual(outputs[0]["room"],outputs[3]["room"])
        self.assertGreater(abs(outputs[0]["energy"]-outputs[3]["energy"]),.001,
                           "Changing only the selected room's filter must reach C++ output")
        self.assertEqual(outputs[0]["room"],outputs[4]["room"])
        self.assertGreater(abs(outputs[0]["energy"]-outputs[4]["energy"]),1e-7,
                           "Changing only the selected room's spread must reach C++ output")
        script="const fs=require('fs'),m=require('./web/model.js');process.stdout.write(m.serialize(m.createScene(JSON.parse(fs.readFileSync(0,'utf8')))));"
        small=ROOT/"build/workflow/scene16.json"
        result=subprocess.run(["node","-e",script],input=json.dumps(generate_quantum(16)),text=True,cwd=ROOT,check=True,capture_output=True)
        small.write_text(result.stdout)
        result=subprocess.run([str(STUDIO),"--scene-smoke",str(small)],capture_output=True,text=True,check=True)
        self.assertEqual(json.loads(result.stdout)["rooms"],16)
        scene=json.loads((ROOT/"build/workflow/scene.json").read_text())
        scene["transitions"][0]=[0]*64
        path=ROOT/"build/workflow/invalid.json";path.write_text(json.dumps(scene))
        result=subprocess.run([str(STUDIO),"--scene-smoke",str(path)],capture_output=True,text=True)
        self.assertEqual(result.returncode,1)
        self.assertIn("Incomplete probability row",result.stderr)
        scene=json.loads((ROOT/"build/workflow/scene.json").read_text())
        scene["performance"].pop("loop")
        path.write_text(json.dumps(scene))
        result=subprocess.run([str(STUDIO),"--scene-smoke",str(path)],capture_output=True,text=True)
        self.assertEqual(result.returncode,1)
        self.assertIn("loop",result.stderr)
        scene=json.loads((ROOT/"build/workflow/scene.json").read_text())
        scene["rooms"][0]["lowpass_hz"]=799
        path.write_text(json.dumps(scene))
        result=subprocess.run([str(STUDIO),"--scene-smoke",str(path)],capture_output=True,text=True)
        self.assertEqual(result.returncode,1);self.assertIn("High cut",result.stderr)
        scene["rooms"][0]["lowpass_hz"]=800;scene["rooms"][0]["highpass_hz"]=900
        path.write_text(json.dumps(scene))
        result=subprocess.run([str(STUDIO),"--scene-smoke",str(path)],capture_output=True,text=True)
        self.assertEqual(result.returncode,1);self.assertIn("Low cut must be below High cut",result.stderr)
        scene=json.loads((ROOT/"build/workflow/scene.json").read_text())
        for value in (-.1,1.01,None):
            scene["rooms"][0]["spread"]=value;path.write_text(json.dumps(scene))
            result=subprocess.run([str(STUDIO),"--scene-smoke",str(path)],capture_output=True,text=True)
            self.assertEqual(result.returncode,1);self.assertIn("Spread",result.stderr)
