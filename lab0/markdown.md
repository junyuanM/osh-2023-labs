![](src/R.jfif)

split函数（用Typora打开会高亮）

```cpp

using namespace std

vector<string> Split(string str,const char split)
{

	istringstream iss(str);	

	vector<string> splitstr;

	string buffer;			

	while (getline(iss, buffer, split))	
	{
		splitstr.push_back(buffer);
	}

	return splitstr;
}
```

喜欢的数学公式：欧拉公式（在Typora中显示）

$$
e^{i\pi}+1=0
$$
