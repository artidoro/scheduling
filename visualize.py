import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np
import sys

def gen_graphs(fp):
    print("Generating graphs for {}".format(fp))
    # nested dict indexed by n, bound, m, ["timeout"|"finished"]
    # contains a list of scheduling times
    results = {}
    bounds = []
    ns = []
    ms = []
    def add_result(n, bound, m, opt, time):
        nonlocal results, bounds, ns, ms
        if n not in ns:
            ns.append(n)
        if bound not in bounds:
            bounds.append(bound)
        if m not in ms:
            ms.append(m)
        cur = results
        if n not in cur:
            cur[n] = {}
        cur = cur[n]
        if bound not in cur:
            cur[bound] = {}
        cur = cur[bound]
        if m not in cur:
            cur[m] = {"timeout": [], "finished": []}
        cur = cur[m]
        if opt == -2:
            cur["timeout"].append(time)
        else:
            cur["finished"].append(time)

    def get_results(n, bound, m, status):
        nonlocal results
        try:
            return results[n][bound][m][status]
        except:
            return None

    with open(fp, "r") as f:
        lines = f.readlines()
        for line in lines:
            line = line[:-1]
            l = line.split(',')
            n = int(l[1])
            m = int(l[2])
            opt = int(l[3])
            time = float(l[4])
            bound = l[5]
            add_result(n, bound[1:], m, opt, time)
        bounds.sort()
        ns.sort()
        ms.sort()

    # percent completed
    # one graph per number of machines
    # x-axis is n, bars for both bounds
    for m in ms:
        # a list of sums for each bound
        percents = {bound: [0] * len(ns) for bound in bounds}
        for bound in bounds:
            for nidx in range(len(ns)):
                fails = get_results(ns[nidx], bound, m, "timeout")
                succs = get_results(ns[nidx], bound, m, "finished")
                if fails != None and succs != None:
                    nfails = len(fails)
                    nsuccs = len(succs)
                    percents[bound][nidx] = nsuccs / (nfails + nsuccs) * 100
                else:
                    print("no data", m, bound, ns[nidx], fails, succs)
        fig, ax = plt.subplots()
        width = .8 * (ns[1] - ns[0]) / len(bounds)
        rects = [ax.bar([n+width*i for n in ns], percents[bounds[i]], width)
                 for i in range(len(bounds))]
        ax.set_ylabel("Percent completed")
        ax.set_xlabel("number of vertices")
        ax.set_title("Percent of DAGs scheduled in 1 minute (m = {})".format(m))
        ax.set_xticks([n + width*(len(bounds)-1)/2 for n in ns])
        ax.set_xticklabels([str(n) for n in ns])
        ax.legend(rects, bounds)

        plt.show()

    # plt.boxplot(node_values)
    # plt.title('Results for m=16')
    # plt.xlabel('# of Nodes')
    # plt.ylabel('Time to Schedule')
    # plt.xticks([i for i in range(1,15)], [i for i in range(12,26)])
    # plt.show()

def gen_boxplot(fp, m, x, b='Fujita'):
    times = []
    nodes = {}

    with open(fp, 'r') as f:
        lines = f.readlines()
        for line in lines:
            l = line.split(',')
            l = l[1:] # we don't care about the test file
            node = int(l[0])
            machines = int(l[1])
            schedule_length = int(l[2])
            scheduling_time = float(l[3])
            bound = str(l[4])

            if machines != m:
                continue

            if b not in bound:
                continue

            if schedule_length < 0 or scheduling_time > 0.06:
                continue

            times.append(scheduling_time)
            if node in nodes.keys():
                nodes[node].append(scheduling_time)
            else:
                nodes[node] = []

    node_values = [nodes[i] for i in range(12,26) if i in nodes.keys()]

    meanpointprops = dict(marker='D', markeredgecolor='black',
                      markerfacecolor='firebrick')
    axes = plt.gca()
    # axes.set_ylim([0,3])
    plt.boxplot(node_values, meanprops=meanpointprops)
    plt.xticks([i for i in range(1,15, 2)], [i for i in range(12,26, 2)])
    plt.subplot(gs[x])

def gen_machineplot(fp, b='Fernandez'):
    nodes = {}

    with open(fp, 'r') as f:
        lines = f.readlines()
        for line in lines:
            l = line.split(',')
            l = l[1:] # we don't care about the test file
            node = int(l[0])
            machines = int(l[1])
            schedule_length = int(l[2])
            scheduling_time = float(l[3])
            bound = str(l[4])

            if b in bound:
                continue

            if machines in nodes.keys():
                nodes[machines].append(scheduling_time)
            else:
                nodes[machines] = []

    node_values = [nodes[i] for i in [16, 20, 24, 28, 32] if i in nodes.keys()]

    meanpointprops = dict(marker='D', markeredgecolor='black',
                      markerfacecolor='firebrick')
    plt.boxplot(node_values, meanprops=meanpointprops)
    plt.xlabel('# of Machines')
    plt.ylabel('Time to Schedule')
    plt.xticks([i for i in range(1,6)], [i for i in [16, 20, 24, 28, 32]])
    return plt

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: {} <file>".format(sys.argv[0]))
        sys.exit(1)

    fig = plt.figure() 
    gs = gridspec.GridSpec(2, 2)

    gen_boxplot(sys.argv[1], 4, 0, b="Fujita")
    gen_boxplot(sys.argv[1], 8, 1, b="Fujita")
    gen_boxplot(sys.argv[1], 16, 2, b="Fujita")
    gen_boxplot(sys.argv[1], 4, 0, b="Fujita")
    # fig.add_subplot(fuj1) 
    # fig.add_subplot(fuj2) 
    # fig.add_subplot(fuj3) 
    # fig.add_subplot(fuj4)
    fig.suptitle('Fujita Bound on m=4,8,16')
    plt.savefig('Fujita Small.png')

