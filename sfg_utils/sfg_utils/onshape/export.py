#!/usr/bin/env python3
import argparse
import math
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any, cast

import requests

ONSHAPE_API = "https://cad.Onshape.com/api"


def get_onshape_auth() -> tuple[str, str]:
    """Retrieve Onshape API credentials from environment variables."""
    access_key = os.environ.get("ONSHAPE_API_KEY")
    secret_key = os.environ.get("ONSHAPE_API_SECRET")

    if not access_key or not secret_key:
        raise EnvironmentError(
            "Please set ONSHAPE_API_KEY and ONSHAPE_API_SECRET environment variables."
        )

    return (access_key, secret_key)


def parse_onshape_url(url: str) -> tuple[str, str, str, str]:
    """
    Parses an Onshape URL to extract the Document ID, the workspace type, the workspace ID, and the element ID.
    """
    pattern = r"/documents/([a-zA-Z0-9]+)/([wvm])/([a-zA-Z0-9]+)/e/([a-zA-Z0-9]+)"
    match = re.search(pattern, url)

    if not match:
        raise ValueError("Invalid Onshape URL.")

    document_id, branch_type, branch_id, element_id = match.groups()
    return document_id, branch_type, branch_id, element_id


def get_filtered_part_ids(
    document_id: str,
    branch_type: str,
    branch_id: str,
    element_id: str,
    whitelist: list[str] | None,
    blacklist: list[str] | None,
    export_hidden: bool,
) -> list[str]:
    """Queries the part studio and returns a list of internal part IDs."""
    auth = get_onshape_auth()
    headers = {"Accept": "application/vnd.Onshape.v1+json"}

    print("Mapping part names to internal part IDs...")
    url = (
        f"{ONSHAPE_API}/parts/d/{document_id}/{branch_type}/{branch_id}/e/{element_id}"
    )

    response = requests.get(url, auth=auth, headers=headers)
    response.raise_for_status()
    parts_data = response.json()

    try:
        whitelist_patterns = (
            [re.compile(pattern) for pattern in whitelist] if whitelist else []
        )
        blacklist_patterns = (
            [re.compile(pattern) for pattern in blacklist] if blacklist else []
        )
    except re.error as e:
        raise ValueError(f"Invalid regular expression provided: {e}")

    matched_ids = []

    for part in parts_data:
        part_name = part["name"]
        part_id = part["partId"]

        # 1. Check hidden status.
        if not export_hidden and part.get("isHidden", False):
            print(f"  Skipping '{part_name}' (hidden)")
            continue

        # 2. Check blacklist.
        if blacklist_patterns and any(
            pattern.search(part_name) for pattern in blacklist_patterns
        ):
            print(f"  Skipping '{part_name}' (matches blacklist)")
            continue

        # 3. Check whitelist.
        if whitelist_patterns and not any(
            pattern.search(part_name) for pattern in whitelist_patterns
        ):
            continue

        matched_ids.append(part_id)
        print(f"  Including '{part_name}' -> {part_id}")

    if not matched_ids:
        raise ValueError(
            "None of the parts matched the specified whitelist/blacklist filters."
        )

    return matched_ids


def export_from_onshape(
    document_id: str,
    branch_type: str,
    branch_id: str,
    element_id: str,
    distance_tolerance: float,
    angle_tolerance: float,
    download_path: Path,
    whitelist: list[str] | None,
    blacklist: list[str] | None,
    export_hidden: bool,
) -> None:
    """Trigger a glTF translation in Onshape, poll for completion, and download the generated file."""
    auth = get_onshape_auth()
    headers = {"Accept": "application/vnd.Onshape.v1+json"}

    part_ids = None

    if whitelist or blacklist or not export_hidden:
        part_ids = get_filtered_part_ids(
            document_id,
            branch_type,
            branch_id,
            element_id,
            whitelist,
            blacklist,
            export_hidden,
        )

    print(
        f"Requesting glTF export (Distance Tol: {distance_tolerance}mm, Angular Tol: {angle_tolerance}°)..."
    )

    translation_payload = {
        "formatName": "GLTF",
        "storeInDocument": False,
        "resolution": "custom",
        "distanceTolerance": distance_tolerance / 1000.0,
        "angularTolerance": math.radians(angle_tolerance),
        "maximumChordLength": 10.0,
        "yAxisIsUp": True,
    }

    if part_ids:
        translation_payload["partIds"] = ", ".join(part_ids)

    trigger_url = f"{ONSHAPE_API}/partstudios/d/{document_id}/{branch_type}/{branch_id}/e/{element_id}/translations"

    response = requests.post(
        trigger_url, json=cast(Any, translation_payload), auth=auth, headers=headers
    )
    response.raise_for_status()
    translation_id = response.json()["id"]
    print(f"Translation initiated. ID: {translation_id}")
    poll_url = f"{ONSHAPE_API}/translations/{translation_id}"

    while True:
        poll_result = requests.get(poll_url, auth=auth, headers=headers)
        poll_result.raise_for_status()
        state = poll_result.json()["requestState"]

        if state == "DONE":
            print("Translation complete!")
            external_data_ids = poll_result.json().get("resultExternalDataIds", [])

            if not external_data_ids:
                raise Exception("Translation completed but returned no data IDs.")

            result_id = external_data_ids[0]
            break
        elif state == "FAILED":
            raise Exception(f"Onshape translation failed: {poll_result.text}")

        print("Waiting for Onshape to process...")
        time.sleep(5)

    print("Downloading exported file...")
    download_url = f"{ONSHAPE_API}/documents/d/{document_id}/externaldata/{result_id}"
    download_result = requests.get(download_url, auth=auth, stream=True)
    download_result.raise_for_status()

    with open(download_path, "wb") as file:
        for chunk in download_result.iter_content(chunk_size=8192):
            file.write(chunk)

    print(f"Downloaded to '{download_path}'.")


def optimize_mesh(
    input_gltf: Path,
    output_glb: Path,
    simplify_ratio: float | None,
) -> None:
    """Run gltfpack on the input file."""
    print(f"Optimizing '{input_gltf}' with gltfpack, and saving to '{output_glb}'...")

    cmd = [
        "gltfpack",
        "-i",
        str(input_gltf),
        "-o",
        str(output_glb),
        "-kn",
        "-km",
        "-tc",
    ]

    if simplify_ratio is not None:
        print(f"Applying simplification ratio: {simplify_ratio}")
        cmd.extend(["-si", str(simplify_ratio)])

    try:
        subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        print("Optimization complete!")
    except subprocess.CalledProcessError as error:
        print(f"gltfpack failed:\n{error.stderr.decode()}", file=sys.stderr)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--url", required=True, help="Full Onshape URL to the Part Studio"
    )
    parser.add_argument("--out", required=True, type=Path, help="Output .glb file path")
    parser.add_argument(
        "--whitelist",
        nargs="+",
        help="Specific part names or regex patterns to include",
    )
    parser.add_argument(
        "--blacklist",
        nargs="+",
        help="Specific part names or regex patterns to exclude",
    )
    parser.add_argument(
        "--export_hidden",
        action="store_true",
        help="Export hidden parts as well",
    )
    parser.add_argument(
        "--distance_tolerance",
        type=float,
        default=1.0,
        help="Distance tolerance in mm",
    )
    parser.add_argument(
        "--angular_tolerance",
        type=float,
        default=25.0,
        help="Angular tolerance in degrees",
    )
    parser.add_argument(
        "--simplification_ratio",
        type=float,
        default=0.5,
        help="gltfpack simplification ratio",
    )
    args = parser.parse_args()

    try:
        document_id, branch_type, branch_id, element_id = parse_onshape_url(args.url)
        print(
            f"Parsed URL -> Document: {document_id}, Branch Type: {branch_type}, Branch ID: {branch_id}, Element: {element_id}"
        )
    except ValueError as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)

    with tempfile.TemporaryDirectory() as temporary_directory:
        download_path = Path(temporary_directory) / "export.gltf"
        export_from_onshape(
            document_id,
            branch_type,
            branch_id,
            element_id,
            args.distance_tolerance,
            args.angular_tolerance,
            download_path,
            args.whitelist,
            args.blacklist,
            args.export_hidden,
        )
        optimize_mesh(download_path, args.out, args.simplification_ratio)


if __name__ == "__main__":
    main()
