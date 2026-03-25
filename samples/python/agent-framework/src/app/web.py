"""
Flask Web Server
─────────────────
Serves the web UI and exposes API endpoints with SSE streaming
for real-time agent pipeline visualisation.
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
import traceback

from flask import Flask, Response, jsonify, render_template, request

from .documents import load_documents
from .foundry_boot import FoundryConnection, FoundryLocalBootstrapper
from .orchestrator import run_full_workflow, run_sequential
from .tool_demo import run_tool_demo

log = logging.getLogger(__name__)

# ── Global state (set in create_app) ────────────────────────────
_conn: FoundryConnection | None = None
_docs_path: str = "./data"
_docs = None


def create_app(conn: FoundryConnection | None = None) -> Flask:
    """Create and configure the Flask application."""
    global _conn, _docs, _docs_path

    app = Flask(__name__, template_folder="templates")

    _docs_path = os.getenv("DOCS_PATH", "./data")

    if conn is not None:
        _conn = conn
    else:
        boot = FoundryLocalBootstrapper()
        _conn = boot.bootstrap()

    _docs = load_documents(_docs_path)

    # ── Routes ───────────────────────────────────────────

    @app.route("/")
    def index():
        return render_template("index.html")

    @app.route("/api/status")
    def api_status():
        if _conn is None:
            return jsonify({"status": "error", "message": "Not bootstrapped"})
        return jsonify({
            "status": "ok",
            "model_alias": _conn.model_alias,
            "model_id": _conn.model_id,
            "endpoint": _conn.endpoint,
            "documents": _docs.file_count if _docs else 0,
        })

    @app.route("/api/run", methods=["POST"])
    def api_run():
        """Run the research workflow and stream events via SSE."""
        if _conn is None:
            return jsonify({"status": "error", "message": "Not bootstrapped"}), 503

        data = request.get_json(silent=True) or {}
        question = data.get("question", "").strip()
        mode = data.get("mode", "full")

        if not question:
            return jsonify({"status": "error", "message": "No question provided"}), 400

        def generate():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                if mode == "sequential":
                    gen = run_sequential(_conn, _docs, question)
                else:
                    gen = run_full_workflow(_conn, _docs, question)

                # Stream each event as it arrives instead of buffering
                agen = gen.__aiter__()
                while True:
                    try:
                        evt = loop.run_until_complete(agen.__anext__())
                        yield f"data: {json.dumps(evt)}\n\n"
                    except StopAsyncIteration:
                        break
            except Exception as exc:
                log.exception("Workflow error")
                yield f"data: {json.dumps({'type': 'error', 'message': 'An internal error occurred. Check server logs for details.'})}\n\n"
            finally:
                asyncio.set_event_loop(None)
                loop.close()

        return Response(
            generate(),
            mimetype="text/event-stream",
            headers={
                "Cache-Control": "no-cache",
                "Connection": "keep-alive",
                "X-Accel-Buffering": "no",
            },
        )

    @app.route("/api/tools", methods=["POST"])
    def api_tools():
        """Run the tool demo and return results."""
        if _conn is None:
            return jsonify({"status": "error", "message": "Not bootstrapped"}), 503

        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        try:
            results = loop.run_until_complete(run_tool_demo(_conn))
            return jsonify({"status": "ok", "results": results})
        except Exception as exc:
            log.exception("Tool demo error")
            return jsonify({"status": "error", "message": "An internal error occurred. Check server logs for details."}), 500
        finally:
            asyncio.set_event_loop(None)
            loop.close()

    @app.route("/api/documents")
    def api_documents():
        return jsonify({
            "status": "ok",
            "file_count": _docs.file_count if _docs else 0,
            "chunk_count": len(_docs.chunks) if _docs else 0,
            "files": list({c.source for c in _docs.chunks}) if _docs else [],
        })

    @app.route("/api/demos")
    def api_demos():
        from .demos import list_demos
        return jsonify({
            "status": "ok",
            "demos": [
                {
                    "id": d.id,
                    "name": d.name,
                    "description": d.description,
                    "icon": d.icon,
                    "category": d.category,
                    "tags": d.tags,
                    "suggested_prompt": d.suggested_prompt,
                }
                for d in list_demos()
            ],
        })

    @app.route("/api/demo/<demo_id>/run", methods=["POST"])
    def api_demo_run(demo_id: str):
        """Run a specific demo and stream results via SSE."""
        from .demos import get_demo

        if _conn is None:
            return jsonify({"status": "error", "message": "Not bootstrapped"}), 503

        demo = get_demo(demo_id)
        if demo is None:
            return jsonify({"status": "error", "message": f"Demo '{demo_id}' not found"}), 404

        data = request.get_json(silent=True) or {}
        prompt = data.get("prompt", "").strip()
        if not prompt:
            return jsonify({"status": "error", "message": "No prompt provided"}), 400

        def generate():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                yield f"data: {json.dumps({'type': 'step_start', 'agent': demo.name})}\n\n"
                result = loop.run_until_complete(demo.runner(_conn, prompt))
                yield f"data: {json.dumps({'type': 'step_done', 'agent': demo.name, 'output': result.get('response', ''), 'elapsed': result.get('elapsed')})}\n\n"
                yield f"data: {json.dumps({'type': 'complete', 'report': result.get('response', '')})}\n\n"
            except Exception as exc:
                log.exception("Demo error: %s", demo_id)
                yield f"data: {json.dumps({'type': 'error', 'message': 'An internal error occurred. Check server logs for details.'})}\n\n"
            finally:
                asyncio.set_event_loop(None)
                loop.close()

        return Response(
            generate(),
            mimetype="text/event-stream",
            headers={
                "Cache-Control": "no-cache",
                "Connection": "keep-alive",
                "X-Accel-Buffering": "no",
            },
        )

    return app
