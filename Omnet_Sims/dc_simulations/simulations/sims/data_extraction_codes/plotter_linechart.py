#!/usr/bin/env python3

import argparse
import os
import re
from collections import OrderedDict

import matplotlib
matplotlib.use("Agg")  # Headless/server mode: never try to open a GUI window.

import matplotlib.pyplot as plt
import numpy as np


RESULT_RE = re.compile(
    r"Mean CCT \(s\):\s*"
    r"([-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?)"
    r"\s*,\s*p99 CCT \(s\):\s*"
    r"([-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?)"
)


def is_separator(line):
    stripped = line.strip()
    return bool(stripped) and set(stripped) <= set("-=_*")


def parse_input_file(input_file_path):
    """
    Parse text like:

        ---------------------------------------------
        ring_bcast
        Mean CCT (s): 0.0014, p99 CCT (s): 0.0020

        ---------------------------------------------
        tree_bcast
        Mean CCT (s): 0.0047, p99 CCT (s): 0.0060

    Returns:
        OrderedDict:
            category -> list of (mean_cct, p99_cct)
    """
    if not os.path.isfile(input_file_path):
        raise IOError("Input file does not exist: {}".format(input_file_path))

    data = OrderedDict()
    current_category = None

    with open(input_file_path, "r") as f:
        for raw_line in f:
            line = raw_line.strip()

            if not line:
                continue

            if is_separator(line):
                continue

            match = RESULT_RE.search(line)

            if match:
                if current_category is None:
                    raise ValueError(
                        "Found a CCT result before finding a category name:\n{}".format(line)
                    )

                mean_cct = float(match.group(1))
                p99_cct = float(match.group(2))

                if current_category not in data:
                    data[current_category] = []

                data[current_category].append((mean_cct, p99_cct))

                # The extractor normally prints the category immediately before
                # each result. Reset this so unrelated later text is not reused
                # accidentally as a category.
                current_category = None
                continue

            # Any nonempty, non-separator, non-result line is a category candidate.
            # This matches the extractor output where `print(category)` appears
            # immediately before the CCT result.
            current_category = line

    if not data:
        raise ValueError(
            "No CCT results were found in '{}'.\n"
            "Expected lines like:\n"
            "Mean CCT (s): 0.001, p99 CCT (s): 0.002".format(input_file_path)
        )

    return data


def aggregate_by_case(values, num_cases, category):
    """
    The extractor can print multiple repetitions for each x-axis case.

    Expected ordering for each category:
        case 1 rep 1
        case 1 rep 2
        ...
        case 2 rep 1
        case 2 rep 2
        ...

    If there are N cases and M total values for a category, M must be
    divisible by N. The repetition count is inferred as M / N.

    Returns:
        mean_values, p99_values, repetitions
    """
    total = len(values)

    if total < num_cases:
        raise ValueError(
            "Category '{}' has only {} result(s), but {} case(s) were provided: {}"
            .format(category, total, num_cases, num_cases)
        )

    if total % num_cases != 0:
        raise ValueError(
            "Category '{}' has {} result(s), which cannot be evenly mapped "
            "to {} case(s).\n"
            "Make sure every case has the same number of repetitions."
            .format(category, total, num_cases)
        )

    repetitions = total // num_cases

    mean_values = []
    p99_values = []

    for case_index in range(num_cases):
        start = case_index * repetitions
        end = start + repetitions
        group = values[start:end]

        group_means = [item[0] for item in group]
        group_p99s = [item[1] for item in group]

        mean_values.append(float(np.mean(group_means)))
        p99_values.append(float(np.mean(group_p99s)))

    return mean_values, p99_values, repetitions


def make_output_path(output_fig_path, suffix):
    output_path = "{}_{}.pdf".format(output_fig_path, suffix)

    parent = os.path.dirname(output_path)
    if parent and not os.path.isdir(parent):
        os.makedirs(parent)

    return output_path


def plot_metric(
    aggregated_data,
    cases,
    x_axis_title,
    metric_index,
    ylabel,
    output_path
):
    x = np.arange(len(cases))

    fig, ax = plt.subplots(figsize=(7.2, 4.5))

    markers = ["o", "s", "^", "D", "v", "P", "X", "<", ">", "*", "+", "|"]

    for index, (category, values) in enumerate(aggregated_data.items()):
        y_values = values[metric_index]

        translated_category = category
        if 'ring' in category:
            translated_category = 'Ring'
        elif 'tree' in category:
            translated_category = 'Binary Tree'
        elif 'optimal' in category and 'optireduce' in category:
            translated_category = 'Optireduce + Optimal Multicast'
        elif 'optimal' in category:
            translated_category = 'Optimal Multicast'
        elif 'orca' in category:
            translated_category = 'Orca'
        elif 'elmo' in category:
            translated_category = 'Elmo'
        elif 'peel' in category and 'optireduce' in category:
            translated_category = 'Optireduce + Peel'
        elif 'peel' in category:
            translated_category = 'Peel'
        elif 'optireduce' in category:
            translated_category = 'Optireduce'
        elif 'sharp' in category:
            translated_category = 'SHArP'

        ax.plot(
            x,
            y_values,
            marker=markers[index % len(markers)],
            linewidth=2.0,
            markersize=6,
            label=translated_category
        )

    ax.set_xticks(x)
    ax.set_xticklabels([str(case) for case in cases])
    ax.set_xlabel(x_axis_title)
    ax.set_ylabel(ylabel)

    ax.grid(True, linestyle="--", alpha=0.35)
    ax.legend(loc="upper left", frameon=False)

    fig.tight_layout()
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)


def print_summary(aggregated_data, cases):
    print("")
    print("Parsed/aggregated CCT values:")
    print("---------------------------------------------")

    for category, values in aggregated_data.items():
        mean_values = values[0]
        p99_values = values[1]
        repetitions = values[2]

        print(category)
        print("  repetitions per case: {}".format(repetitions))

        for i, case in enumerate(cases):
            print(
                "  case {}: Mean CCT = {}, p99 CCT = {}"
                .format(case, mean_values[i], p99_values[i])
            )

        print("")


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Read CCT results from a text file and create mean-CCT and "
            "p99-CCT line plots. Designed for headless servers."
        )
    )

    parser.add_argument(
        "--input_file_path",
        required=True,
        help="Input text file, e.g. archive/sample.txt"
    )

    parser.add_argument(
        "--output_fig_path",
        required=True,
        help=(
            "Output figure prefix, e.g. figs/flow_size. "
            "Creates <prefix>_meancct.pdf and <prefix>_tailcct.pdf"
        )
    )

    parser.add_argument(
        "--cases",
        nargs="+",
        required=True,
        help="Ordered x-axis cases, e.g. --cases 35 55 75 95"
    )

    parser.add_argument(
        "--x_axis_title",
        required=True,
        help='X-axis title, e.g. "Flow Size (KB)"'
    )

    args = parser.parse_args()

    raw_data = parse_input_file(args.input_file_path)

    aggregated_data = OrderedDict()

    for category, values in raw_data.items():
        mean_values, p99_values, repetitions = aggregate_by_case(
            values,
            len(args.cases),
            category
        )

        # tuple layout:
        #   0 -> mean values
        #   1 -> p99 values
        #   2 -> inferred repetitions per case
        aggregated_data[category] = (
            mean_values,
            p99_values,
            repetitions
        )

    print_summary(aggregated_data, args.cases)

    mean_output_path = make_output_path(
        args.output_fig_path,
        "meancct"
    )

    tail_output_path = make_output_path(
        args.output_fig_path,
        "tailcct"
    )

    plot_metric(
        aggregated_data=aggregated_data,
        cases=args.cases,
        x_axis_title=args.x_axis_title,
        metric_index=0,
        ylabel="Mean CCT (s)",
        output_path=mean_output_path
    )

    plot_metric(
        aggregated_data=aggregated_data,
        cases=args.cases,
        x_axis_title=args.x_axis_title,
        metric_index=1,
        ylabel="p99 CCT (s)",
        output_path=tail_output_path
    )

    print("Saved:")
    print("  {}".format(mean_output_path))
    print("  {}".format(tail_output_path))


if __name__ == "__main__":
    main()
