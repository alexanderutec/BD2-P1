#ifndef SEQUENTIALFILE_H
#define SEQUENTIALFILE_H

#include <string>
#include <fstream>
#include <vector>
#include <algorithm>
#include "Registros.h"
#include <ctgmath>
#include <queue>

using namespace std;

const string BinSuffix = ".dat";
const string AuxSuffix = "_aux" + BinSuffix;
const string CSVSuffix = ".csv";

struct NextLabel
{
    int nextRow;
    char nextRowFile;
};

bool operator==(NextLabel a, NextLabel b)
{
    return a.nextRow == b.nextRow && a.nextRowFile == b.nextRowFile;
}

ostream &operator<<(ostream &os, NextLabel n)
{
    os.write((char *)&n, sizeof(NextLabel));
    return os;
}

istream &operator>>(istream &is, NextLabel &n)
{
    is.read((char *)&n, sizeof(NextLabel));
    return is;
}

template <typename T>
class SequentialFile
{
private:
    int row_sizeof, K_max_aux, eliminados = 0;
    string base_path;
    void CSV_Loader(string _base_path);
    void reorganize();

public:
    SequentialFile(string _base_path);
    void add(T reg);
    template <typename Key_t>
    vector<T> rangeSearch(Key_t begin_key, Key_t end_key);
    template <typename Key_t>
    void remove(Key_t key);
    template <typename Key_t>
    T search(Key_t key);
    void loadAll(vector<T> &regs, vector<NextLabel> &labs);
};

template <typename T>
template <typename Key_t>
void SequentialFile<T>::remove(Key_t key)
{
    if (eliminados >= K_max_aux)
        reorganize();

    fstream file(base_path + BinSuffix, ios::in | ios::binary);
    file.seekg(0, ios::end);
    int l = 0, m;
    int r = (file.tellg() - row_sizeof - sizeof(NextLabel)) / row_sizeof;

    T row_reg;
    const T key_reg(key);
    NextLabel row_label;
    bool row_in_aux = false;
    while (l <= r)
    {
        m = (l + r) / 2;
        file.seekg(sizeof(NextLabel) + m * row_sizeof, ios::beg);
        file >> row_reg;
        if (key_reg > row_reg)
            l = m + 1;
        else if (key_reg < row_reg)
            r = m - 1;
        else
        {
            file >> row_label;
            break;
        }
    }

    if (l > r)
    {
        m = 0;
        row_in_aux = true;
        while (file >> row_reg)
        {
            if (key_reg == row_reg)
            {
                file >> row_label;
                break;
            }
            else
                file.ignore(NextLabel);
            m++;
        }
        if (key_reg != row_reg)
        {
            throw("La llave no existe");
        }
    }

    file.close();

    eliminados++;
}

template <typename T>
void SequentialFile<T>::reorganize()
{
    fstream fileData(base_path + BinSuffix, ios::out | ios::in | ios::binary);
    ifstream fileAux(base_path + AuxSuffix, ios::in | ios::binary);
    fileData.seekg(0, ios::end);
    int total_rows = (int(fileData.tellg()) - sizeof(NextLabel)) / row_sizeof + K_max_aux;

    fileData.seekg(0, ios::beg);
    queue<T> save_regs;
    T write_reg, save_reg;
    NextLabel write_label, pass_label;

    fileData >> pass_label;
    for (write_label = {1, 'd'}; pass_label.nextRow == -1; write_label.nextRow++)
    {
        fileData << write_label;
        if (pass_label.nextRowFile == 'd')
        {
            fileAux.seekg((pass_label.nextRow - 1) * row_sizeof + sizeof(NextLabel), ios::beg);
            if (save_regs.size() > 0)
            {
                fileData >> save_reg >> pass_label;
                save_regs.push(save_reg);
                fileData.seekg(-1 * row_sizeof, ios::cur);
                fileData << save_regs.top();
            }
            else
            {
                fileData.ignore(sizeof(T));
            }
        }
        else if (pass_label.nextRowFile == 'a')
        {
            fileAux.seekg((pass_label.nextRow - 1) * row_sizeof, ios::beg);
            fileAux >> write_reg >> pass_label;
            fileData >> save_reg;
            save_regs.push(save_reg);
            fileData.seekg(-1 * sizeof(T), ios::cur);
            fileData << write_reg;
        }
    }

    fileData << pass_label;

    fileAux.close();
    fileData.close();
}

template <typename T>
void SequentialFile<T>::loadAll(vector<T> &regs, vector<NextLabel> &labs)
{
    ifstream fileData(base_path + BinSuffix, ios::in | ios::binary);

    NextLabel l;
    T r;
    fileData >> l;
    labs.push_back(l);
    while (fileData >> r >> l)
    {
        regs.push_back(r);
        labs.push_back(l);
    }
    fileData.close();
    fileData.open(base_path + AuxSuffix, ios::in | ios::binary);
    while (fileData >> r >> l)
    {
        regs.push_back(r);
        labs.push_back(l);
    }
    fileData.close();
}

template <typename T>
template <typename Key_t>
vector<T> SequentialFile<T>::rangeSearch(Key_t begin_key, Key_t end_key)
{
    T key_group[2] = {T(begin_key), T(end_key)};

    if (key_group[0] > key_group[1])
        throw("La llave de inicio es mayor a la final");
    else if (key_group[0] == key_group[1])
        return vector<T>{search<Key_t>(begin_key)};

    T row_group[2];
    NextLabel label_group[2];

    ifstream fileData(base_path + BinSuffix, ios::in | ios::binary);
    ifstream fileAux(base_path + AuxSuffix, ios::in | ios::binary);
    for (int i = 0; i < 2; i++)
    {
        int l = 0;
        fileData.seekg(0, ios::end);
        int r = (fileData.tellg() - row_sizeof - sizeof(NextLabel)) / row_sizeof;
        while (l <= r)
        {
            int m = (l + r) / 2;
            fileData.seekg(sizeof(NextLabel) + m * row_sizeof, ios::beg);
            fileData >> row_group[i];
            if (key_group[i] > row_group[i])
                l = m + 1;
            else if (key_group[i] < row_group[i])
                r = m - 1;
            else
            {
                fileData >> label_group[i];
                break;
            }
        }
        if (key_group[i] != row_group[i])
        {
            fileAux.seekg(0, ios::beg);

            while (fileAux >> row_group[i])
            {
                if (row_group[i] == key_group[i])
                {
                    fileAux >> label_group[i];
                    break;
                }
                else
                    fileAux.ignore(sizeof(NextLabel));
            }

            if (row_group[i] != key_group[i])
                i ? throw("Llave de cierre no encontrada") : throw("Llave de inicio no encontrada");
        }
    }

    vector<T> result{row_group[0]};

    while (!(label_group[0] == label_group[1]))
    {
        if (label_group[0].nextRowFile == 'd')
        {
            fileData.seekg((label_group[0].nextRow - 1) * row_sizeof + sizeof(NextLabel), ios::beg);
            fileData >> row_group[0] >> label_group[0];
        }
        else
        {
            fileAux.seekg((label_group[0].nextRow - 1) * row_sizeof, ios::beg);
            fileAux >> row_group[0] >> label_group[0];
        }
        result.push_back(row_group[0]);
    }

    fileAux.close();
    fileData.close();

    return result;
}

template <typename T>
template <typename Key_t>
T SequentialFile<T>::search(Key_t key)
{
    ifstream file(base_path + BinSuffix, ios::in | ios::binary);

    file.seekg(0, ios::end);
    int l = 0;
    int r = (file.tellg() - row_sizeof - sizeof(NextLabel)) / row_sizeof;

    T result, objective(key);

    while (l <= r)
    {
        int m = (l + r) / 2;
        file.seekg(sizeof(NextLabel) + m * row_sizeof, ios::beg);
        file >> result;
        if (objective > result)
            l = m + 1;
        else if (objective < result)
            r = m - 1;
        else
        {
            file.close();
            return result;
        }
    }
    file.close();

    file.open(base_path + AuxSuffix, ios::in | ios::binary);

    while (file >> result)
    {
        if (result == objective)
        {
            file.close();
            return result;
        }
        file.ignore(sizeof(NextLabel));
    }

    file.close();
    throw("No encontrado");
    return T();
}

template <typename T>
void SequentialFile<T>::add(T reg)
{
    fstream fileAux(base_path + AuxSuffix, ios::in | ios::out | ios::binary);
    fileAux.seekg(0, ios::end);
    if (int(fileAux.tellg()) / row_sizeof >= K_max_aux)
    {
        fileAux.close();
        reorganize();
    }
    else
        fileAux.close();
    fileAux.open(base_path + AuxSuffix, ios::in | ios::out | ios::binary);

    fstream fileData(base_path + BinSuffix, ios::in | ios::out | ios::binary);

    fileData.seekg(0, ios::end);
    int l = 0, r = (fileData.tellg() - row_sizeof - sizeof(NextLabel)) / row_sizeof;
    int n_regs = r + 1;
    T prev;
    int prev_ptr_offset;
    bool prev_in_aux = false;
    while (l <= r)
    {
        int m = (l + r) / 2;
        fileData.seekg(sizeof(NextLabel) + m * row_sizeof, ios::beg);
        fileData >> prev;
        if (reg > prev)
            l = m + 1;
        else if (reg < prev)
            r = m - 1;
        else
            break;
    }

    if (l <= r)
        throw("La llave debe ser diferente");

    NextLabel prev_ptr;
    if (r == -1)
    {
        fileData.seekg(0, ios::beg);
        fileData >> prev_ptr;
    }
    else
    {
        fileData.seekg(l * row_sizeof - sizeof(T), ios::beg);
        fileData >> prev >> prev_ptr;
        prev_ptr_offset = fileData.tellg() - sizeof(NextLabel);
    }

    T aux_prev;
    fileAux.seekg(0, ios::beg);
    while (fileAux >> aux_prev)
    {
        if (reg == aux_prev)
            throw("La llave debe ser diferente");
        else if ((r == -1 || aux_prev > prev) && aux_prev < reg)
        {
            if (r == -1)
                r--;
            prev = aux_prev;
            fileAux >> prev_ptr;
            prev_ptr_offset = fileAux.tellg() - sizeof(NextLabel);
            if (!prev_in_aux)
                prev_in_aux = true;
        }
        else
            fileAux.ignore(sizeof(NextLabel));
    }

    fileAux.close();
    fileAux.open(base_path + AuxSuffix, ios::out | ios::binary | ios::app);
    fileAux.seekp(0, ios::end);
    fileAux << reg << prev_ptr;
    fileAux.close();

    fileAux.open(base_path + AuxSuffix, ios::in | ios::out | ios::binary);
    fileAux.seekg(0, ios::end);
    prev_ptr = {((int)fileAux.tellp()) / row_sizeof, 'a'};

    if (!prev_in_aux)
    {
        if (r == -1)
            fileData.seekp(0, ios::beg);
        else
            fileData.seekp(prev_ptr_offset, ios::beg);
        fileData << prev_ptr;
    }
    else
    {
        fileAux.seekp(prev_ptr_offset, ios::beg);
        fileAux << prev_ptr;
    }

    fileData.close();
    fileAux.close();
}

template <typename T>
void SequentialFile<T>::CSV_Loader(string csv)
{
    ifstream csv_file(csv, ios::in);
    string temp;
    vector<T> registros;
    while (getline(csv_file, temp))
    {
        T registro;
        registro.readCSVLine(temp);
        registros.push_back(registro);
    }
    sort(registros.begin(), registros.end());

    csv_file.close();

    ofstream ogData(base_path + BinSuffix, ios::out | ios::binary | ios::app);
    NextLabel siguiente;
    for (siguiente = {1, 'd'}; siguiente.nextRow - 1 < registros.size(); siguiente.nextRow++)
    {
        ogData << siguiente;
        ogData << registros[siguiente.nextRow - 1];
    }
    siguiente.nextRow = -1;
    ogData << siguiente;

    ogData.close();
}

template <typename T>
SequentialFile<T>::SequentialFile(string _base_path)
    : base_path(_base_path), row_sizeof(sizeof(T) + sizeof(NextLabel))
{
    const string binaryDB = base_path + BinSuffix, auxDB = base_path + AuxSuffix;
    fstream fCreate(auxDB, ios::in | ios::out | ios::app);
    fCreate.close();
    fCreate.open(binaryDB, ios::in | ios::out | ios::app);
    fCreate.seekp(0, ios::end);
    if (fCreate.tellp() == 0)
    {
        fCreate.close();
        const string csvDB = base_path + CSVSuffix;
        CSV_Loader(csvDB);
    }
    fCreate.close();

    fCreate.open(binaryDB, ios::in | ios::binary);
    fCreate.seekg(0, ios::end);
    K_max_aux = static_cast<int>(log2((int(fCreate.tellg()) - sizeof(NextLabel)) / row_sizeof));
    fCreate.close();
}

#endif