#include "KeyframeFinder.h"
#include <cmath>
#include <algorithm>
#include <cassert>
#define fo(i, a, b) for (int i = a; i <= b; i ++)

using namespace std;

vector<int> KeyframeFinder::getKeyframes(
        int numberOfFrames,
        int numberOfKeyframes
)
{
    assert(numberOfKeyframes <= maxNumberOfKeyframes_);
    vector<int> res;
    res.clear();
    int i = numberOfFrames;
    for (int j = numberOfKeyframes; j > 0; j --)
    {
        i = g[i][j];
        // k + 1 is the keyframe, so here we should write i + 1
        res.push_back(i + 1);
    }
    for (int i = 0; i < res.size(); i ++)
    {
        // convert back to be indexed from 0
        res[i] -= 1;
    }
    sort(res.begin(), res.end());
    return res;
}
