#!/bin/bash

if [[ -z "${CFLAGS}" ]]
then
	CFLAGS=""
fi

MapFile="busybox_unstripped.map"
MakeFileHost="MakefileHost.mk"
BusyBoxRebuild="busybox-rebuild"
AppletLibOCmd="libbb/.appletlib.o.cmd"
BusyBoxUnstrippedOut="busybox_unstripped.out"

my_arg0="$0"
my_which=`which "${my_arg0}"`
my_path=`readlink -f "${my_which}"`
my_dir=`dirname "${my_path}"`
my_name=`basename "${my_path}"`

if [[ -n "${TEMP}" ]]
then
	temp_dir="${TEMP}"
fi

if [[ ( -z ${temp_dir} ) && ( -n "${TMP}" ) ]]
then
	temp_dir="${TMP}"
fi

if [[ ( -z "${temp_dir}" ) ]]
then
	temp_dir=/tmp
fi

work_dir="${temp_dir}/${my_name}-$$-{`cat /proc/sys/kernel/random/uuid`}"

if ! mkdir -p "${work_dir}"
then
	echo "${my_name}: Can not create work directory. work_dir=${work_dir}"
	exit 2
fi

function work_dir_clean() {
	rm -rf "${work_dir}"
}

map_file="${my_dir}/${MapFile}"
source_files_tmp="${work_dir}/source_files.txt"

if ! ( grep '^[[:space:]]\+.text[[:space:]]' "${map_file}" \
	| grep -v '/usr/lib' \
	| grep -v 'build-in.o' \
	| gawk -F '[[:space:]]+' '{print $5}' \
	| sed -n 's!\(.*\)/lib.*a(\(.*\))!\1/\2! p' \
	| sed -n 's![.]o!.c! p' \
	> "${source_files_tmp}" \
	)
then
	echo "${my_name}: Can not filter map file. map_file=${map_file}"
	exit 2
fi

build_command_file="${my_dir}/${AppletLibOCmd}"
ccflags_tmp="${work_dir}/ccflags.txt"

if ! ( grep '^[[:space:]]*cmd_' "${build_command_file}" \
	| gawk -F ':=' '{print $2}' \
	| sed 's/[[:space:]]\+-/\n-/g' \
	| grep '^[[:space:]]*-' \
	| grep -v \
		-e '^-c' \
		-e '^-o' \
		-e '-Wp' \
	| sed '/KBUILD/ s/\\#/#/' \
	> "${ccflags_tmp}" \
	)
then
	echo "${my_name}: Can not filter build command file. build_command_file=${build_command_file}"
	exit 2
fi

link_command_file="${my_dir}/${BusyBoxUnstrippedOut}"
libs_tmp="${work_dir}/linklibs.txt"

if ! ( grep '^[[:space:]]*gcc' "${link_command_file}" \
	| sed 's/[[:space:]]\+-/\n-/g' \
	| grep '^[[:space:]]*-' \
	| grep '^-l' \
	> "${libs_tmp}" \
	)
then
	echo "${my_name}: Can not filter link command file. link_command_file=${link_command_file}"
	exit 2
fi

echo > "${MakeFileHost}"

cat << EOF > "${MakeFileHost}"
# MakefileHost.mk
# Test build busybox with simplified Makefile.
EOF
echo "# BEGIN: source_files" >> "${MakeFileHost}"
echo "source_files = \\" >> "${MakeFileHost}"
gawk '{print "\t" $0 " \\"}' "${source_files_tmp}" >> "${MakeFileHost}"
echo  "" >> "${MakeFileHost}"
echo "# Keep avobe line intentionally blank." >> "${MakeFileHost}"
echo "# END: source_files" >> "${MakeFileHost}"

cat << EOF >> "${MakeFileHost}"
${BusyBoxRebuild}: \$(source_files)
	\$(CC) -o ${BusyBoxRebuild} \\
EOF

echo -n $'\t\t' >> "${MakeFileHost}"
echo   "${CFLAGS} \\" >> "${MakeFileHost}"

sed -n 's/^/\t\t/ p' "${ccflags_tmp}" | sed -n 's/$/ \\/ p' >> "${MakeFileHost}"

cat << EOF >> "${MakeFileHost}"
		\$(source_files) \\
EOF
sed -n 's/^/\t\t/ p' "${libs_tmp}" | sed -n 's/$/ \\/ p' >> "${MakeFileHost}"
echo $'\t\t' >> "${MakeFileHost}"
cat << EOF >> "${MakeFileHost}"
clean:
	rm "${BusyBoxRebuild}"
EOF

work_dir_clean
