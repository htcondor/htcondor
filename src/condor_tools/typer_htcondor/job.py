# job.py
import typer
job_app = typer.Typer()

@job_app.command()
def status():
    typer.echo("Checking job status...")

@job_app.command()
def submit():
    typer.echo("Submitting job...")
