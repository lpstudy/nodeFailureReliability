#include "typedef.h"
#include <iostream>
#include <vector>
#include <time.h>
#include <math.h>
#include <string>
#include <fstream>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

#include "profile.h"
#include <Windows.h>

using namespace std;

const char* lableCreate = "Create";
const char* lableDelete = "Delete";
const char* lableRead = "Read";
const char* lableWrite = "Write";
const char* lableHot = "Hot";
const char* lableCold = "Cold";




class HdfsFile
{
public:
	int index;
	long fileSize;
	bool isHot;
};


class Configure{
public:
	Configure(){
		const char* section = "hotFile";
		const char* inFile = ".\\hotFileGenerator.ini";

		file_max_count = GetPrivateProfileInt(section, "totalFileCount",100, inFile);
		file_hot_percentage = GetPrivateProfileInt(section, "hotFilePercentage",10, inFile);
		file_hot_distributed = GetPrivateProfileInt(section, "hotFileDistribution",1, inFile);
		out_file_count = GetPrivateProfileInt(section, "outFileCount",1, inFile);
		file_max_size = GetPrivateProfileInt(section, "fileMaxSize",10485760, inFile);
		file_min_size = GetPrivateProfileInt(section, "fileMinSize",512, inFile);
		file_size_distribution = GetPrivateProfileInt(section, "fileSizeDistribution",1, inFile);
		

		GetPrivateProfileString(section, "hotFileNamePrefix", "hot_", hot_file_prefix, 256, inFile);
		GetPrivateProfileString(section, "coldFileNamePrefix","cold_", cold_file_prefix, 256, inFile);
		GetPrivateProfileString(section, "outFileName","fileSeq.txt", out_file_name, 256, inFile);
		
		pefect_size_count = 0;
		init();

	}
	~Configure(){
		clear();
	}
public:
	void splitString(string s,vector<string>& ret)  
	{  
		size_t last = 0;
		size_t index=s.find_first_of('.',last);
		while (index!=std::string::npos)
		{
			ret.push_back(s.substr(last,index-last));
			last=index+1;
			index=s.find_first_of('.',last);
		}
		if (index-last>0)
		{
			ret.push_back(s.substr(last,index-last));
		}
	}
public:
	void init(){
		gsl_rng_env_setup();
		//gsl_rng_default_seed = ((unsigned long)(time(NULL)));
		T = gsl_rng_default; 
		r = gsl_rng_alloc(T);


		vector<string> str;
		string fileSeqName;
		char buffer[256];

		splitString(out_file_name,str);
		for (int i = 0;i<out_file_count;i++)
		{
			
			sprintf(buffer,"%s%04d",str[0].c_str(), i);
			string temp(buffer);
			if (str.size() != 1)
			{
				fileSeqName = temp + "." + str[1];
			}
			else{
				fileSeqName = temp + "." + "txt";
			}
			outFile[i].open(fileSeqName.c_str());
		}

	}
	void clear(){
		gsl_rng_free(r);
		for (int i = 0;i<out_file_count;i++)
		{
			outFile[i].close();
		}
		
	}
public:
	void Run(){
		CreateFile();
		HotFile();
	}
	void Print(){
		cout<<"Perfect count:"<<pefect_size_count<<endl;
	}
public:
	void CreateFile(){
		long hotFileCount = file_max_count * file_hot_percentage/100;
		
		for (int i = 0; i < file_max_count; ++i)
		{
			HdfsFile file;
			file.fileSize = randFileSize();
			if(file.fileSize >= 512*1024){
				pefect_size_count++;
			}
			file.isHot = (i < hotFileCount? true : false);
			file.index = i;
			
			fileNameVec.push_back(file);
		}
		
		vector<int> fileCountVec(out_file_count);
		for (int i = 0; i < file_max_count; ++i)
		{
			int index = i % out_file_count;
			fileCountVec[index]++;
		}
		for (int i = 0; i < out_file_count; ++i){
			outFile[i] << fileCountVec[i]<<endl;
		}
		for (int i = 0; i < file_max_count; ++i)
		{
			int index = i % out_file_count;
			string fileName = getFileNameByFile(fileNameVec[i]);
			outFile[index] << lableCreate <<" "<<fileName<<" "<<fileNameVec[i].fileSize<<endl;
		}
	}
	string getFileNameByFile(HdfsFile file){
		string fileName = (file.isHot ? hot_file_prefix: cold_file_prefix);
		char buf[100];
		sprintf(buf, "%d", file.index);
		fileName = fileName + getFormatSizeString(file.fileSize) + "_" + buf;
		return fileName;
	}
	void HotFile(){
		long hotFileCount = file_max_count * file_hot_percentage/100;

		vector<int> fileCountVec(out_file_count);
		for (int i = 0; i < hotFileCount; ++i)
		{
			int index = i % out_file_count;
			fileCountVec[index]++;
		}

		int index = 0;
		for (int i = 0; i < out_file_count; ++i){
			outFile[i] << fileCountVec[i]<<endl;
		}
		int true_i = -1;

		vector<int> eachHotFileSize(out_file_count);
		vector<int> eachTotalFileSize(out_file_count);
		for (int i = 0; i < file_max_count; ++i)
		{
			if(fileNameVec[i].isHot){
				true_i++;
				int index = true_i % out_file_count;
				string fileName = getFileNameByFile(fileNameVec[i]);
				outFile[index] << lableHot <<" "<<fileName<<" "<<fileNameVec[i].fileSize<<endl;
				eachHotFileSize[index] += fileNameVec[i].fileSize;
			}
			int index = i % out_file_count;
			eachTotalFileSize[index] += fileNameVec[i].fileSize;
		}
		for (int i = 0; i < out_file_count; ++i)
		{
			outFile[i]<<endl;
			outFile[i]<<"Total:"<<getFormatSizeString(eachTotalFileSize[i])<< " Hot:"<<getFormatSizeString(eachHotFileSize[i])<<endl;
		}
	}
	string getFormatSizeString(long aSize){
		double size = aSize;
		char buf[256];

		if(size >= 1024*1024){
			size = size/1024/1024;
			sprintf(buf, "%.2fGB", size);	
		}else if (size >= 1024){
			size = size / 1024;
			sprintf(buf, "%.2fMB", size);	
		}else{
			sprintf(buf, "%.2fKB", size);	
		}

		return string(buf);
	}
	long randFileSize(){
		double fileSize = 0;
		if (file_size_distribution == 1)
		{
			fileSize = gsl_rng_uniform_int(r, file_max_size-file_min_size) + file_min_size;
		}else if(file_size_distribution == 2){
			fileSize = gsl_ran_lognormal(r, 11, 3.3);
			//fileSize *= 1000;
		}else{
			cout<<"Unsupported file size distribution, exit..."<<endl;
			exit(0);
		}

		while(fileSize > file_max_size || fileSize < file_min_size){
			fileSize = randFileSize();
		}
		return (long)fileSize;
	}
public:
	long file_max_count;
	int file_hot_percentage;
	int file_hot_distributed;
	int out_file_count;
	long file_max_size;
	long file_min_size;
	char hot_file_prefix[100];
	char cold_file_prefix[100];
	char out_file_name[256];
	int file_size_distribution;
	vector<HdfsFile> fileNameVec;
	int pefect_size_count;
private:
	ofstream outFile[100];
	const gsl_rng_type *T; //定义type变量 
	gsl_rng *r; 

};


int main(){
	Configure g_config;
	g_config.Run();
	g_config.Print();
	return 0;
}