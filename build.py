#!/usr/bin/env python3

import subprocess
import argparse
from pathlib import Path


def cmd(*args, **kwargs):
	print(args, kwargs)
	p = subprocess.Popen(args, **kwargs)
	return p.wait()

def git(*args, **kwargs):
	if cmd("git", *args, **kwargs) != 0:
		raise Exception(f"git command failed: {' '.join(args)} {kwargs}")

def pull_git_dependency(dir, url, *, branch = "main"):
	if dir.exists():
		git("fetch", cwd=dir)
		git("checkout", branch, cwd=dir)
		git("merge", f"origin/{branch}", "--ff-only", cwd=dir)
	else:
		git("clone", url, dir, "-b", branch)

def cmake_var_def_args(vars):
	for name, value in vars.items():
		if isinstance(value, Path):
			# necessary so that cmake deals with \ correctly
			yield f"-D{name}:PATH={value}"
		else:
			yield f"-D{name}={value}"

def cmake_configure(build_dir, source_dir, configs, **vars):
	if cmd("cmake", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", *tuple(cmake_var_def_args(vars)), source_dir, cwd=build_dir) != 0:
		raise Exception(f"cmake command failed: {build_dir} {source_dir} {configs} {vars}")
	
def ninja(build_dir, *targets):
	if cmd("ninja", *targets, cwd=build_dir) != 0:
		raise Exception(f"ninja failed: {build_dir} {targets}")


class LLVM:
	def __init__(self, dir):
		dir.mkdir(exist_ok=True)
		self.source_dir = dir/"src"
		self.build_dir = dir/"build"

	def pull(self):
		pull_git_dependency(self.source_dir, "https://github.com/llvm/llvm-project.git")

	def configure(self, configs):
		self.build_dir.mkdir(exist_ok=True)

		cmake_configure(self.build_dir, self.source_dir/"llvm", configs,
			LLVM_OPTIMIZED_TABLEGEN=True,
			LLVM_TARGETS_TO_BUILD="AArch64;X86",
			LLVM_ENABLE_PROJECTS="clang;lld;lldb",
			LLVM_ENABLE_BINDINGS=False,
			LLVM_INCLUDE_TOOLS=True,
			LLVM_INCLUDE_TESTS=False,
			LLVM_INCLUDE_BENCHMARKS=False,
			LLVM_INCLUDE_EXAMPLES=False,
			LLVM_INCLUDE_DOCS=False,
			CLANG_ENABLE_ARCMT=False,
			CLANG_ENABLE_STATIC_ANALYZER=False,
			CLANG_INCLUDE_TESTS=False,
			CLANG_INCLUDE_DOCS=False,
		)

	def build(self, configs):
		ninja(self.build_dir)


def dependencies(this_dir):
	deps_dir = this_dir/"deps"
	deps_dir.mkdir(exist_ok=True)

	stdlib_dir = deps_dir/"stdlib"

	llvm = LLVM(deps_dir/"llvm")

	yield llvm


def build(deps, configs):
	for dep in deps:
		dep.configure(configs)
		dep.build(configs)


def pull(deps):
	for dep in deps:
		dep.pull()


def main(args):
	this_dir = Path(__file__).parent

	def cmdargs():
		yield dependencies(this_dir)
		if hasattr(args, "configs"):
			yield args.configs

	args.command(*tuple(cmdargs()))


if __name__ == "__main__":
	args = argparse.ArgumentParser()
	sub_args = args.add_subparsers(required=True)

	def add_command(name, function):
		args = sub_args.add_parser(name)
		args.set_defaults(command=function)
		return args

	build_cmd = add_command("build", build)
	build_cmd.add_argument("-cfg", "--config", action="append", dest="configs", default=["Release"])

	pull_cmd = add_command("pull", pull)

	main(args.parse_args())
