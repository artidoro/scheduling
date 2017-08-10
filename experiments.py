import subprocess
import sys

n_dags = 30
small_machines = [4,8,16]
timeout = 60
bounds = {"Fujita": [],
          "Fernandez": ["FB=1"]}

def make_clean():
    clean = subprocess.run(["make", "clean"],
                           stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
    if clean.returncode != 0:
        print("make clean failed")
        sys.exit(1)

def make(args=[]):
    make = subprocess.run(["make"] + args,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE,
                          encoding="utf-8")
    if make.returncode != 0:
        print("make failed")
        sys.exit(1)

def bbexps(path, m, t):
    result = subprocess.run(
        ["./bbexps", path, str(m), str(t)],
        stdout=subprocess.PIPE,
        encoding="utf-8")
    return result


def runSmall():
    n_dags = 30
    machines = [4,8,16]
    timeout_skip = 12
    sizes = list(range(12,26))
    for bound in bounds:
        make_clean()
        make(bounds[bound])
        for size in sizes:
            for m in machines:
                n_timeouts = 0
                for dag in range(n_dags):
                    path = "series/data{}01/Pat{}.rcp".format(size, dag)
                    result = bbexps(path, m, timeout)
                    print("{}, {}".format(result.stdout[:-1], bound))
                    if "-2" in result.stdout:
                        n_timeouts += 1
                    else:
                        n_timeouts = 0
                    if n_timeouts >= timeout_skip:
                        break

def runLarge():
    n_dags = 16
    machines = [24, 28, 32, 36, 40]
    for bound in bounds:
        make_clean()
        make(bounds[bound])
        for size in range(100, 155, 5):
            for m in machines:
                for dag in range(n_dags):
                    path = "large_data/data{}01/Pat{}.rcp".format(size, dag)
                    result = bbexps(path, m, timeout)
                    print("{}, {}".format(result.stdout[:-1], bound))

def main():
    runLarge()
    #runSmall()

if __name__ == "__main__":
    main()
