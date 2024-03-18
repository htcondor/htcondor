# job.py
import typer

@job_app.command()(status)
def status():
    typer.echo("Checking job status...")

@job_app.command()(submit)
def submit():
    typer.echo("Submitting job...")

job_app = typer.Typer()