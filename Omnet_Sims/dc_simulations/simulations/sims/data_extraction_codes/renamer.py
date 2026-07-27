from os import listdir, scandir, rename
from os.path import isfile, join
import shutil

list_subdirs = [f.path for f in scandir('.') if f.is_dir()]
old_name_chunk = '_p_bcast'
new_name_chunk = 'p_btree'


for dir in list_subdirs:
    if './.' not in dir and './_' not in dir and 'RECORDS' not in dir and 'FIGS' not in dir:
        onlyfiles = [f for f in listdir(dir) if isfile(join(dir, f)) and old_name_chunk in f]
        print(onlyfiles)
        for i in range(len(onlyfiles)):
            old_splited_name = onlyfiles[i].split(old_name_chunk)
            if len(old_splited_name) > 2:
                raise Exception("Multiple instances of your chunk in the name!")
            new_name = old_splited_name[0] + new_name_chunk + old_splited_name[1]
            print(old_splited_name)
            print(new_name)
            rename(dir + '/' + onlyfiles[i], dir + '/' + new_name)
