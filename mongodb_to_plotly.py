import pymongo
import pandas as pd
import plotly.graph_objects as go

myclient = pymongo.MongoClient("mongodb://localhost:27017/")
mydb = myclient["test"]
mycol = mydb["log"]

df = pd.DataFrame(list(mycol.find()),columns=["datetime","tempSP","tempErr","tempPV","tempExt"])

df['tempSP'] = df['tempSP'].astype(float)
df['tempErr'] = df['tempErr'].astype(float)

df['tempSPMinErr'] = df.tempSP + df.tempErr
df['tempSPMaxErr'] = df.tempSP - df.tempErr

##print(df)

fig = go.Figure()

fig.add_trace(go.Scatter(x=df.datetime, y=df.tempSP,
                    mode='lines',
                    name='tempSP'))
fig.add_trace(go.Scatter(x=df.datetime, y=df.tempSPMaxErr,
                    mode='lines',
                    name='tempSPMaxErr'))
fig.add_trace(go.Scatter(x=df.datetime, y=df.tempSPMinErr,
                    mode='lines',
                    name='tempSPMinErr'))
fig.add_trace(go.Scatter(x=df.datetime, y=df.tempPV,
                    mode='lines',
                    name='tempPV'))
fig.add_trace(go.Scatter(x=df.datetime, y=df.tempExt,
                    mode='lines',
                    name='tempExt'))

fig.update_layout(
    title="Temperature",
    xaxis_title="DateTime (min)",
    yaxis_title="Temperature (°C)",
    font=dict(
        family="Arial",
        size=15,
        color="#fff"
    ),
    paper_bgcolor="#000",
    plot_bgcolor="#000"
)

fig.show()
